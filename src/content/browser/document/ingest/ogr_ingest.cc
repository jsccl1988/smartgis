// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/ingest/ogr_ingest.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "content/browser/document/ingest/china_clip.h"
#include "content/browser/document/ingest/seed_paths.h"
#include "gdal_priv.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "gis/datasource/pipeline/feature_load_pipeline.h"
#include "gis/datasource/pipeline/ogr_feature_load.h"
#include "ogrsf_frmts.h"

namespace content {
namespace detail {
namespace {

constexpr size_t kMaxFeaturesPerLayer = 8000;

bool kind_is_text(const char* kind) {
  return kind && (std::strcmp(kind, "text") == 0 ||
                  std::strcmp(kind, "label") == 0 ||
                  std::strcmp(kind, "anno") == 0);
}

bool kind_is_region(const char* kind) {
  return kind && (std::strcmp(kind, "region") == 0 ||
                  std::strcmp(kind, "area") == 0 ||
                  std::strcmp(kind, "polygon") == 0 ||
                  std::strcmp(kind, "basemap") == 0);
}

bool kind_is_line(const char* kind) {
  return kind && (std::strcmp(kind, "line") == 0 ||
                  std::strcmp(kind, "river") == 0 ||
                  std::strcmp(kind, "corridor") == 0 ||
                  std::strcmp(kind, "road") == 0);
}

bool kind_is_point(const char* kind) {
  return kind && (std::strcmp(kind, "point") == 0 ||
                  std::strcmp(kind, "city") == 0);
}

bool layer_name_is_text(const char* layer_name) {
  return layer_name && (std::strcmp(layer_name, "text") == 0 ||
                        std::strcmp(layer_name, "anno") == 0 ||
                        std::strcmp(layer_name, "label") == 0 ||
                        std::strcmp(layer_name, "注记") == 0);
}

bool has_nonempty_field(const MapFeature& f, const char* key) {
  const char* v = named_field_value(f, key);
  return v && v[0];
}

// Prefer anno (FtAnno), then name, for Chinese annotation labels.
std::string feature_display_name(const MapFeature& f) {
  if (const char* anno = named_field_value(f, "anno")) {
    if (anno[0]) {
      return anno;
    }
  }
  if (const char* name = named_field_value(f, "name")) {
    if (name[0]) {
      return name;
    }
  }
  return {};
}

bool ascii_icontains(const char* hay, const char* needle) {
  if (!hay || !needle || !needle[0]) {
    return false;
  }
  const size_t n = std::strlen(needle);
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    for (; i < n; ++i) {
      unsigned char a = static_cast<unsigned char>(p[i]);
      unsigned char b = static_cast<unsigned char>(needle[i]);
      if (a == 0) {
        return false;
      }
      if (a >= 'A' && a <= 'Z') {
        a = static_cast<unsigned char>(a - 'A' + 'a');
      }
      if (b >= 'A' && b <= 'Z') {
        b = static_cast<unsigned char>(b - 'A' + 'a');
      }
      if (a != b) {
        break;
      }
    }
    if (i == n) {
      return true;
    }
  }
  return false;
}

void ensure_anno_from_name(MapFeature* out) {
  if (!out || out->kind != GeomKind::kText) {
    return;
  }
  if (has_nonempty_field(*out, "anno")) {
    return;
  }
  if (const char* name = named_field_value(*out, "name")) {
    if (name[0]) {
      out->fields.push_back({"anno", name});
    }
  }
}

void apply_kind_override(MapFeature* out, const char* ogr_layer_name) {
  if (!out) {
    return;
  }
  // Point (+ MultiPoint first vertex) + text layer / anno / kind  - kText.
  const bool point_like = out->kind == GeomKind::kPoint ||
                          out->kind == GeomKind::kText;
  if (point_like && !out->points.empty() &&
      (layer_name_is_text(ogr_layer_name) || has_nonempty_field(*out, "anno") ||
       kind_is_text(named_field_value(*out, "kind")))) {
    out->kind = GeomKind::kText;
    ensure_anno_from_name(out);
    return;
  }
  const char* kind = named_field_value(*out, "kind");
  if (kind_is_region(kind) && out->points.size() >= 3) {
    out->kind = GeomKind::kPolygon;
    return;
  }
  if (kind_is_line(kind) && out->points.size() >= 2) {
    out->kind = GeomKind::kLine;
    return;
  }
  if (kind_is_point(kind) && !out->points.empty()) {
    out->kind = GeomKind::kPoint;
  }
}

GeomKind hosted_kind(gis::datasource::OgrPartKind kind) {
  switch (kind) {
    case gis::datasource::OgrPartKind::kLine:
      return GeomKind::kLine;
    case gis::datasource::OgrPartKind::kPolygon:
      return GeomKind::kPolygon;
    case gis::datasource::OgrPartKind::kPoint:
      return GeomKind::kPoint;
  }
  return GeomKind::kPoint;
}

MapFeature hosted_feature_from_part(gis::datasource::OgrFeaturePart&& part) {
  MapFeature feature;
  feature.kind = hosted_kind(part.kind);
  feature.selected = false;
  feature.points.reserve(part.points.size());
  for (const gis::datasource::OgrMapVertex& vertex : part.points) {
    feature.points.push_back({vertex.x, vertex.y});
  }
  feature.fields.reserve(part.fields.size());
  for (gis::datasource::OgrMapField& field : part.fields) {
    feature.fields.push_back({std::move(field.name), std::move(field.value)});
  }
  return feature;
}

// One MapFeature per drawable part. Decode + ring decimation live in
// gis::datasource; this adapter only fills hosted vertices/fields and
// applies kind/anno overrides. China line clip / land-mask / OMT class
// normalize happen offline (testing/data/carto_prep).
// MultiPolygon / MultiLineString must expand every part. Keeping only the
// largest ring leaves Xinjiang/Qinghai (and island archipelagos) as white
// holes while rivers still draw through.
size_t features_from_ogr(OGRFeature* ogr_feat,
                         std::vector<MapFeature>* out,
                         const char* ogr_layer_name) {
  if (!ogr_feat || !out) {
    return 0;
  }
  std::vector<gis::datasource::OgrFeaturePart> loaded;
  gis::datasource::load_ogr_feature_parts(ogr_feat, &loaded);
  out->clear();
  out->reserve(loaded.size());
  for (gis::datasource::OgrFeaturePart& part : loaded) {
    const gis::datasource::OgrPartKind decoded = part.kind;
    MapFeature feature = hosted_feature_from_part(std::move(part));
    apply_kind_override(&feature, ogr_layer_name);
    // Accept on the decoded kind's minimum, after clip and kind override.
    // A line clipped under 2 vertices is dropped even if override retargets it.
    if (decoded == gis::datasource::OgrPartKind::kLine &&
        feature.points.size() < 2) {
      continue;
    }
    if (decoded == gis::datasource::OgrPartKind::kPolygon &&
        feature.points.size() < 3) {
      continue;
    }
    out->push_back(std::move(feature));
  }
  return out->size();
}

}  // namespace

void try_load_accompanying_style(
    const std::function<bool(const std::string&)>& load_style,
    const std::string& path) {
  if (!load_style || path.empty()) {
    return;
  }
  // china_city.style.json keys source-layer area/line/point and disables
  // MapLibre carto_source_layer remap (cream wash + blue scribble / black
  // point squares). Default china seed and --map2d-showcase=china require a
  // null StyleDocument → default_carto_style_json. Refuse that file even when
  // it sits beside the gpkg as stem.style.json.
  auto is_china_city_style = [](const std::string& style_path) {
    // path_stem("…/china_city.style.json") → "china_city.style".
    // Also catch "china_city.gpkg.style.json" and case variants by scanning
    // the basename for "china_city.style".
    const std::string stem = path_stem(style_path);
    auto ascii_lower_eq = [](const std::string& s, const char* expect) {
      const size_t n = std::strlen(expect);
      if (s.size() != n) {
        return false;
      }
      for (size_t i = 0; i < n; ++i) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
          c = static_cast<char>(c - 'A' + 'a');
        }
        if (c != expect[i]) {
          return false;
        }
      }
      return true;
    };
    if (ascii_lower_eq(stem, "china_city.style")) {
      return true;
    }
    // Basename contains china_city.style (e.g. china_city.gpkg.style).
    std::string lower = stem;
    for (char& c : lower) {
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
    }
    return lower.find("china_city.style") != std::string::npos;
  };
  auto try_load = [&](const std::string& style_path) {
    if (is_china_city_style(style_path)) {
      return false;
    }
    return load_style(style_path);
  };
  if (try_load(path + ".style.json")) {
    return;
  }
  const size_t slash = path.find_last_of("/\\");
  const std::string dir =
      slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
  const std::string stem = path_stem(path);
  if (!stem.empty()) {
    try_load(dir + stem + ".style.json");
  }
}

bool ingest_ogr_path(LayerStore* store, const std::string& path) {
  if (!store) {
    return false;
  }
  // register_gdal_driver sets GDAL_DRIVER_PATH so AutoLoadDrivers does not
  // LoadLibrary CRT DLLs from the harness cwd (heap-corrupt / failed GPKG).
  static std::once_flag gdal_register_once;
  std::call_once(gdal_register_once, [] {
    (void)gis::datasource::register_gdal_driver();
  });
  GDALDatasetUniquePtr ds(GDALDataset::Open(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY));
  if (!ds) {
    return false;
  }
  const int layer_count = ds->GetLayerCount();
  if (layer_count <= 0) {
    return false;
  }

  std::vector<MapLayer> loaded;
  loaded.reserve(static_cast<size_t>(layer_count));
  size_t total_features = 0;
  for (int li = 0; li < layer_count; ++li) {
    OGRLayer* ogr_layer = ds->GetLayer(li);
    if (!ogr_layer) {
      continue;
    }
    const char* lname = ogr_layer->GetName();
    // china_city historically shipped a parallel text layer (same names at
    // y+0.08°) that double-drew under default carto "label" mapping. Skip it.
    if (path_looks_like_china_city(path) && layer_name_is_text(lname)) {
      continue;
    }
    MapLayer layer;
    layer.id = path + "#" + (lname && lname[0] ? lname : std::to_string(li));
    layer.name = (lname && lname[0]) ? lname : path_stem(path);
    layer.visible = true;
    layer.kind = content::LayerKind::kVector;
    layer.role = infer_layer_role(layer.name);

    // mogu-style: serial GetNextFeature → parallel decode → ordered sink.
    // max_features caps OGR rows read; sink still caps MapFeature parts.
    size_t taken = 0;
    gis::datasource::FeatureLoadOptions opts;
    opts.max_features = kMaxFeaturesPerLayer;
    opts.serial_threshold = 64;
    gis::datasource::load_ogr_layer_pipeline<std::vector<MapFeature>>(
        ogr_layer,
        [lname](OGRFeature* feat, std::vector<MapFeature>* parts) {
          return features_from_ogr(feat, parts, lname) > 0;
        },
        [&](std::vector<MapFeature>&& parts) {
          for (MapFeature& part : parts) {
            if (taken >= kMaxFeaturesPerLayer) {
              break;
            }
            part.id = store->next_feature_id();
            layer.features.push_back(std::move(part));
            ++taken;
            ++total_features;
          }
        },
        opts);

    if (!layer.features.empty()) {
      loaded.push_back(std::move(layer));
    }
  }
  if (loaded.empty() || total_features == 0) {
    return false;
  }
  store->replace_layers(std::move(loaded));
  if (store->layers().size() == 1) {
    split_layers_by_kind_field(store);
  }
  return true;
}

void split_layers_by_kind_field(LayerStore* store) {
  if (!store) {
    return;
  }
  // Hold the store mutex for the whole split. Moving features out of
  // store->layers() without a lock races Display layout fingerprint
  // (scene_layers_fingerprint bit_cast AV on 0xDD freed point buffers).
  std::lock_guard<std::recursive_mutex> lock(store->mutex());
  bool has_kind = false;
  for (const MapLayer& layer : store->layers()) {
    for (const MapFeature& f : layer.features) {
      if (named_field_value(f, "kind")) {
        has_kind = true;
        break;
      }
    }
    if (has_kind) {
      break;
    }
  }
  if (!has_kind) {
    return;
  }

  MapLayer regions;
  regions.id = "china.area";
  regions.name = "area";
  regions.visible = true;
  regions.kind = content::LayerKind::kVector;
  regions.role = LayerRole::kLandSurface;
  MapLayer lines;
  lines.id = "china.line";
  lines.name = "line";
  lines.visible = true;
  lines.kind = content::LayerKind::kVector;
  MapLayer points;
  points.id = "china.point";
  points.name = "point";
  points.visible = true;
  points.kind = content::LayerKind::kVector;
  MapLayer texts;
  texts.id = "china.text";
  texts.name = "text";
  texts.visible = true;
  texts.kind = content::LayerKind::kVector;

  for (MapLayer& layer : store->layers()) {
    for (MapFeature& f : layer.features) {
      apply_kind_override(&f, layer.name.c_str());
      if (f.kind == GeomKind::kText) {
        texts.features.push_back(std::move(f));
      } else if (f.kind == GeomKind::kLine) {
        lines.features.push_back(std::move(f));
      } else if (f.kind == GeomKind::kPoint) {
        points.features.push_back(std::move(f));
      } else {
        regions.features.push_back(std::move(f));
      }
    }
  }

  std::vector<MapLayer> next;
  if (!regions.features.empty()) {
    next.push_back(std::move(regions));
  }
  if (!lines.features.empty()) {
    next.push_back(std::move(lines));
  }
  if (!points.features.empty()) {
    next.push_back(std::move(points));
  }
  if (!texts.features.empty()) {
    next.push_back(std::move(texts));
  }
  if (next.empty()) {
    return;
  }
  // Recursive: replace_layers takes the same mutex.
  store->replace_layers(std::move(next));
}

}  // namespace detail
}  // namespace content

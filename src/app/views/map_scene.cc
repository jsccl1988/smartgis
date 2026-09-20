// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/map_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string_view>

#include "content/public/feature_attrs.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/ogr_text_encoding.h"
#include "gis/world/land_mask.h"
#include "tool/camera_nav.h"

namespace app {
namespace {

constexpr size_t kMaxFeaturesPerLayer = 8000;

std::string path_stem(const std::string& path) {
  if (path.empty()) {
    return {};
  }
  size_t begin = path.find_last_of("/\\");
  begin = (begin == std::string::npos) ? 0 : begin + 1;
  size_t end = path.find_last_of('.');
  if (end == std::string::npos || end < begin) {
    end = path.size();
  }
  return path.substr(begin, end - begin);
}

MapScene::GeomKind geom_from_tool(const char* tool_id, tool::DraftKind kind) {
  if (tool_id) {
    if (std::strncmp(tool_id, "draw.line", 9) == 0) {
      return MapScene::GeomKind::kLine;
    }
    if (std::strncmp(tool_id, "draw.poly", 9) == 0 ||
        std::strncmp(tool_id, "draw.rect", 9) == 0) {
      return MapScene::GeomKind::kPolygon;
    }
  }
  switch (kind) {
    case tool::DraftKind::kLineString:
      return MapScene::GeomKind::kLine;
    case tool::DraftKind::kPolygon:
    case tool::DraftKind::kRect:
      return MapScene::GeomKind::kPolygon;
    default:
      return MapScene::GeomKind::kPoint;
  }
}

double dist2(double ax, double ay, double bx, double by) {
  const double dx = ax - bx;
  const double dy = ay - by;
  return dx * dx + dy * dy;
}

void append_ring(OGRLineString* ring, std::vector<MapScene::Vertex>* out) {
  if (!ring || !out) {
    return;
  }
  const int n = ring->getNumPoints();
  if (n <= 0) {
    return;
  }
  // Cap ring density so GDI Polygon / paint stays bounded on prefecture packs.
  constexpr int kMaxRingPoints = 2048;
  int step = 1;
  if (n > kMaxRingPoints) {
    step = n / kMaxRingPoints;
    if (step < 1) {
      step = 1;
    }
  }
  for (int i = 0; i < n; i += step) {
    // Flip Y so screen +Y is down while GIS +Y stays north-up after fit.
    out->push_back({ring->getX(i), -ring->getY(i)});
  }
  if ((n - 1) % step != 0) {
    out->push_back({ring->getX(n - 1), -ring->getY(n - 1)});
  }
}

// Keep the longest contiguous run inside leftover mainland lon/lat
// (map space = lon / -lat). Applies only to china_city "line" layers so
// Natural Earth river stubs past provincial land are dropped without
// affecting arbitrary non-China line datasets.
void clip_china_city_line_to_mainland(std::vector<MapScene::Vertex>* pts,
                                      const char* ogr_layer_name) {
  if (!pts || pts->size() < 2 || !ogr_layer_name) {
    return;
  }
  if (std::strcmp(ogr_layer_name, "line") != 0) {
    return;
  }
  constexpr double kMinLon = 73.0;
  constexpr double kMaxLon = 135.0;
  // Stored Y is -lat → mainland lat 18..54 becomes Y -54..-18.
  constexpr double kMinY = -54.0;
  constexpr double kMaxY = -18.0;
  auto inside = [&](const MapScene::Vertex& p) {
    return p.x >= kMinLon && p.x <= kMaxLon && p.y >= kMinY && p.y <= kMaxY;
  };
  size_t best_begin = 0;
  size_t best_len = 0;
  size_t i = 0;
  const size_t n = pts->size();
  while (i < n) {
    while (i < n && !inside((*pts)[i])) {
      ++i;
    }
    const size_t begin = i;
    while (i < n && inside((*pts)[i])) {
      ++i;
    }
    const size_t len = i - begin;
    if (len > best_len) {
      best_len = len;
      best_begin = begin;
    }
  }
  if (best_len < 2) {
    // Foreign-only stub on the china_city line layer — drop it.
    pts->clear();
    return;
  }
  if (best_len == n) {
    return;
  }
  std::vector<MapScene::Vertex> kept(
      pts->begin() + static_cast<std::ptrdiff_t>(best_begin),
      pts->begin() + static_cast<std::ptrdiff_t>(best_begin + best_len));
  *pts = std::move(kept);
}

const char* field_value(const MapScene::Feature& f, const char* key) {
  if (!key) {
    return nullptr;
  }
  for (const MapScene::Field& field : f.fields) {
    if (field.name == key) {
      return field.value.c_str();
    }
  }
  return nullptr;
}

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

bool has_nonempty_field(const MapScene::Feature& f, const char* key) {
  const char* v = field_value(f, key);
  return v && v[0];
}

// Prefer anno (SmtFtAnno), then name, for Chinese annotation labels.
std::string feature_display_name(const MapScene::Feature& f) {
  if (const char* anno = field_value(f, "anno")) {
    if (anno[0]) {
      return anno;
    }
  }
  if (const char* name = field_value(f, "name")) {
    if (name[0]) {
      return name;
    }
  }
  return {};
}

void ensure_anno_from_name(MapScene::Feature* out) {
  if (!out || out->kind != MapScene::GeomKind::kText) {
    return;
  }
  if (has_nonempty_field(*out, "anno")) {
    return;
  }
  if (const char* name = field_value(*out, "name")) {
    if (name[0]) {
      out->fields.push_back({"anno", name});
    }
  }
}

void apply_kind_override(MapScene::Feature* out, const char* ogr_layer_name) {
  if (!out) {
    return;
  }
  // Point (+ MultiPoint first vertex) + text layer / anno / kind → kText.
  const bool point_like = out->kind == MapScene::GeomKind::kPoint ||
                          out->kind == MapScene::GeomKind::kText;
  if (point_like && !out->points.empty() &&
      (layer_name_is_text(ogr_layer_name) || has_nonempty_field(*out, "anno") ||
       kind_is_text(field_value(*out, "kind")))) {
    out->kind = MapScene::GeomKind::kText;
    ensure_anno_from_name(out);
    return;
  }
  const char* kind = field_value(*out, "kind");
  if (kind_is_region(kind) && out->points.size() >= 3) {
    out->kind = MapScene::GeomKind::kPolygon;
    return;
  }
  if (kind_is_line(kind) && out->points.size() >= 2) {
    out->kind = MapScene::GeomKind::kLine;
    return;
  }
  if (kind_is_point(kind) && !out->points.empty()) {
    out->kind = MapScene::GeomKind::kPoint;
  }
}

void label_anchor(const MapScene::Feature& f, double* mx, double* my) {
  if (!mx || !my || f.points.empty()) {
    return;
  }
  if (f.kind == MapScene::GeomKind::kPoint ||
      f.kind == MapScene::GeomKind::kText) {
    *mx = f.points[0].x;
    *my = f.points[0].y;
    return;
  }
  double sx = 0;
  double sy = 0;
  for (const MapScene::Vertex& p : f.points) {
    sx += p.x;
    sy += p.y;
  }
  const double n = static_cast<double>(f.points.size());
  *mx = sx / n;
  *my = sy / n;
}

void copy_ogr_fields(OGRFeature* ogr_feat, MapScene::Feature* out) {
  if (!ogr_feat || !out) {
    return;
  }
  out->fields.clear();
  OGRFeatureDefn* defn = ogr_feat->GetDefnRef();
  if (!defn) {
    return;
  }
  const int field_count = defn->GetFieldCount();
  for (int i = 0; i < field_count && static_cast<int>(out->fields.size()) < 12;
       ++i) {
    if (!ogr_feat->IsFieldSetAndNotNull(i)) {
      continue;
    }
    OGRFieldDefn* fd = defn->GetFieldDefn(i);
    if (!fd) {
      continue;
    }
    const char* fname = fd->GetNameRef();
    out->fields.push_back(
        {fname && fname[0] ? fname : "field",
         gis::datasource::ogr_bytes_to_utf8(ogr_feat->GetFieldAsString(i))});
  }
}

bool fill_polygon_feature(OGRPolygon* poly, MapScene::Feature* out,
                          const char* ogr_layer_name) {
  if (!poly || !out) {
    return false;
  }
  out->kind = MapScene::GeomKind::kPolygon;
  out->points.clear();
  out->selected = false;
  if (OGRLinearRing* ext = poly->getExteriorRing()) {
    append_ring(ext, &out->points);
  }
  apply_kind_override(out, ogr_layer_name);
  return out->points.size() >= 3;
}

bool fill_line_feature(OGRLineString* line, MapScene::Feature* out,
                       const char* ogr_layer_name) {
  if (!line || !out) {
    return false;
  }
  out->kind = MapScene::GeomKind::kLine;
  out->points.clear();
  out->selected = false;
  append_ring(line, &out->points);
  clip_china_city_line_to_mainland(&out->points, ogr_layer_name);
  apply_kind_override(out, ogr_layer_name);
  return out->points.size() >= 2;
}

// One MapScene::Feature per drawable part. MultiPolygon / MultiLineString must
// expand every part — keeping only the largest ring leaves Xinjiang/Qinghai
// (and island archipelagos) as white holes while rivers still draw through.
size_t features_from_ogr(OGRFeature* ogr_feat,
                         std::vector<MapScene::Feature>* out,
                         const char* ogr_layer_name) {
  if (!ogr_feat || !out) {
    return 0;
  }
  out->clear();
  OGRGeometry* geom = ogr_feat->GetGeometryRef();
  if (!geom || geom->IsEmpty()) {
    return 0;
  }

  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPoint) {
    MapScene::Feature f;
    copy_ogr_fields(ogr_feat, &f);
    auto* pt = geom->toPoint();
    f.kind = MapScene::GeomKind::kPoint;
    f.points.push_back({pt->getX(), -pt->getY()});
    apply_kind_override(&f, ogr_layer_name);
    out->push_back(std::move(f));
    return 1;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    MapScene::Feature f;
    copy_ogr_fields(ogr_feat, &f);
    if (!fill_line_feature(geom->toLineString(), &f, ogr_layer_name)) {
      return 0;
    }
    out->push_back(std::move(f));
    return 1;
  }
  if (flat == wkbPolygon) {
    MapScene::Feature f;
    copy_ogr_fields(ogr_feat, &f);
    if (!fill_polygon_feature(geom->toPolygon(), &f, ogr_layer_name)) {
      return 0;
    }
    out->push_back(std::move(f));
    return 1;
  }
  if (flat == wkbMultiPoint) {
    auto* multi = geom->toMultiPoint();
    if (!multi || multi->getNumGeometries() < 1) {
      return 0;
    }
    MapScene::Feature f;
    copy_ogr_fields(ogr_feat, &f);
    auto* pt = multi->getGeometryRef(0)->toPoint();
    f.kind = MapScene::GeomKind::kPoint;
    f.points.push_back({pt->getX(), -pt->getY()});
    apply_kind_override(&f, ogr_layer_name);
    out->push_back(std::move(f));
    return 1;
  }
  if (flat == wkbMultiLineString) {
    auto* multi = geom->toMultiLineString();
    if (!multi || multi->getNumGeometries() < 1) {
      return 0;
    }
    const int ngeom = multi->getNumGeometries();
    out->reserve(static_cast<size_t>(ngeom));
    for (int i = 0; i < ngeom; ++i) {
      OGRGeometry* part = multi->getGeometryRef(i);
      if (!part || wkbFlatten(part->getGeometryType()) != wkbLineString) {
        continue;
      }
      MapScene::Feature f;
      copy_ogr_fields(ogr_feat, &f);
      if (fill_line_feature(part->toLineString(), &f, ogr_layer_name)) {
        out->push_back(std::move(f));
      }
    }
    return out->size();
  }
  if (flat == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    if (!multi || multi->getNumGeometries() < 1) {
      return 0;
    }
    const int ngeom = multi->getNumGeometries();
    out->reserve(static_cast<size_t>(ngeom));
    for (int i = 0; i < ngeom; ++i) {
      OGRGeometry* part = multi->getGeometryRef(i);
      if (!part || wkbFlatten(part->getGeometryType()) != wkbPolygon) {
        continue;
      }
      MapScene::Feature f;
      copy_ogr_fields(ogr_feat, &f);
      if (fill_polygon_feature(part->toPolygon(), &f, ogr_layer_name)) {
        out->push_back(std::move(f));
      }
    }
    return out->size();
  }
  return 0;
}

}  // namespace

MapScene::MapScene() = default;

void MapScene::clear() {
  layers_.clear();
  active_layer_id_.clear();
  selected_id_ = {};
  pan_x_ = 0;
  pan_y_ = 0;
  scale_ = 1.0;
  last_open_was_ogr_ = false;
}

std::vector<std::string> china_seed_relative_paths() {
  // Prefer prefecture china_city (SmartGis.exe-like overview) over schematic
  // china_plp (~46 features). Keep plp + views sample as last-resort fallbacks.
  return {
      "china_city.gpkg",
      "china_city.geojson",
      "testing\\data\\china_city.gpkg",
      "testing\\data\\china_city.geojson",
      "..\\testing\\data\\china_city.gpkg",
      "..\\testing\\data\\china_city.geojson",
      "..\\..\\testing\\data\\china_city.gpkg",
      "..\\..\\testing\\data\\china_city.geojson",
      "china_plp.geojson",
      "testing\\data\\china_plp.geojson",
      "..\\testing\\data\\china_plp.geojson",
      "..\\..\\testing\\data\\china_plp.geojson",
      "views_ogr_sample.geojson",
      "testing\\data\\views_ogr_sample.geojson",
      "..\\testing\\data\\views_ogr_sample.geojson",
      "..\\..\\testing\\data\\views_ogr_sample.geojson",
  };
}

COLORREF map_scene_map_bg_color() {
  // Baidu-like soft ocean behind land.
  return RGB(170, 211, 223);
}

COLORREF map_scene_river_color() {
  return RGB(100, 160, 208);
}

COLORREF map_scene_admin_stroke_color() {
  return RGB(196, 190, 176);
}

COLORREF map_scene_point_fill_color() {
  return RGB(90, 110, 130);
}

COLORREF map_scene_area_fill_color(const char* adcode, uint32_t feature_id) {
  // Unified Baidu land wash — boundaries carry identity, not choropleth fills.
  (void)adcode;
  (void)feature_id;
  return RGB(245, 243, 233);
}

bool MapScene::try_bootstrap_china_plp() {
  char exe_dir[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return false;
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (exe_dir[i] == '\\' || exe_dir[i] == '/') {
      exe_dir[i + 1] = '\0';
      break;
    }
  }
  for (const std::string& rel : china_seed_relative_paths()) {
    const std::string cand = std::string(exe_dir) + rel;
    DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES ||
        (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      continue;
    }
    if (open_path(cand)) {
      return true;
    }
  }
  return false;
}

void MapScene::seed_default() {
  if (try_bootstrap_china_plp()) {
    return;
  }
  clear();
  Layer layer;
  layer.id = "layer.demo";
  layer.name = "Demo layer";
  layer.visible = true;
  add_sample_features(&layer, "demo");
  active_layer_id_ = layer.id;
  layers_.push_back(std::move(layer));
}

bool MapScene::compute_extent(double* min_x, double* min_y, double* max_x,
                              double* max_y) const {
  if (!min_x || !min_y || !max_x || !max_y) {
    return false;
  }
  bool have = false;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  for (const Layer& layer : layers_) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      for (const Vertex& p : f.points) {
        if (!have) {
          minx = maxx = p.x;
          miny = maxy = p.y;
          have = true;
        } else {
          minx = std::min(minx, p.x);
          miny = std::min(miny, p.y);
          maxx = std::max(maxx, p.x);
          maxy = std::max(maxy, p.y);
        }
      }
    }
  }
  if (!have) {
    return false;
  }
  *min_x = minx;
  *min_y = miny;
  *max_x = maxx;
  *max_y = maxy;
  return true;
}

bool MapScene::has_china_extent() const {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  if (!compute_extent(&minx, &miny, &maxx, &maxy)) {
    return false;
  }
  // Stored as lon / -lat. Prefecture packs may include Xinjiang west of 70E,
  // northern Heilongjiang (~53–59N in some sources), and South China Sea
  // islands (~3N). Keep a loose envelope; kind counts prove PLP content.
  if (minx < 60.0 || maxx > 145.0 || minx > maxx) {
    return false;
  }
  const double south = -maxy;
  const double north = -miny;
  size_t regions = 0;
  size_t lines = 0;
  size_t points = 0;
  size_t texts = 0;
  for (const Layer& layer : layers_) {
    for (const Feature& f : layer.features) {
      switch (f.kind) {
        case GeomKind::kPolygon:
          ++regions;
          break;
        case GeomKind::kLine:
          ++lines;
          break;
        case GeomKind::kPoint:
          ++points;
          break;
        case GeomKind::kText:
          ++texts;
          break;
      }
    }
  }
  return south >= 3.0 && north <= 60.0 && south < north && regions >= 3 &&
         lines >= 2 && points >= 3 && texts >= 1;
}

bool MapScene::open_path(const std::string& path) {
  const std::string stem = path_stem(path);
  if (stem.empty()) {
    return false;
  }
  if (ingest_ogr_path(path)) {
    last_open_was_ogr_ = true;
    fit_extent(800, 600);
    return true;
  }
  last_open_was_ogr_ = false;
  // Fallback keeps Open → Catalog → paint wired when OGR cannot open the file.
  Layer* existing = find_layer(path);
  if (existing) {
    existing->name = stem;
    existing->visible = true;
    existing->features.clear();
    add_sample_features(existing, stem);
    active_layer_id_ = path;
    return false;
  }
  Layer layer;
  layer.id = path;
  layer.name = stem;
  layer.visible = true;
  add_sample_features(&layer, stem);
  active_layer_id_ = layer.id;
  layers_.push_back(std::move(layer));
  return false;
}

bool MapScene::ingest_ogr_path(const std::string& path) {
  GDALAllRegister();
  GDALDatasetUniquePtr ds(GDALDataset::Open(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR));
  if (!ds) {
    return false;
  }
  const int layer_count = ds->GetLayerCount();
  if (layer_count <= 0) {
    return false;
  }

  std::vector<Layer> loaded;
  loaded.reserve(static_cast<size_t>(layer_count));
  size_t total_features = 0;
  for (int li = 0; li < layer_count; ++li) {
    OGRLayer* ogr_layer = ds->GetLayer(li);
    if (!ogr_layer) {
      continue;
    }
    const char* lname = ogr_layer->GetName();
    Layer layer;
    layer.id = path + "#" + (lname && lname[0] ? lname : std::to_string(li));
    layer.name = (lname && lname[0]) ? lname : path_stem(path);
    layer.visible = true;

    ogr_layer->ResetReading();
    size_t taken = 0;
    while (taken < kMaxFeaturesPerLayer) {
      OGRFeatureUniquePtr feat(ogr_layer->GetNextFeature());
      if (!feat) {
        break;
      }
      std::vector<Feature> parts;
      if (features_from_ogr(feat.get(), &parts, lname) == 0) {
        continue;
      }
      for (Feature& part : parts) {
        if (taken >= kMaxFeaturesPerLayer) {
          break;
        }
        part.id = next_feature_id();
        layer.features.push_back(std::move(part));
        ++taken;
        ++total_features;
      }
      if (taken >= kMaxFeaturesPerLayer) {
        break;
      }
    }
    if (!layer.features.empty()) {
      loaded.push_back(std::move(layer));
    }
  }
  if (loaded.empty() || total_features == 0) {
    return false;
  }
  // Replace in place: clear first so a failed prior document cannot leave
  // overlapping Layer storage while |loaded| is moved (debug CRT has tripped
  // RtlValidateHeap on ~vector<Layer> after assign).
  layers_.clear();
  layers_.swap(loaded);
  active_layer_id_ = layers_.front().id;
  selected_id_ = {};
  // Multi-layer GPKG already uses real OGR names (area/line/point/text).
  // Only regroup single-layer packs that encode type via kind=.
  if (layers_.size() == 1) {
    split_layers_by_kind_field();
  }
  return true;
}

void MapScene::split_layers_by_kind_field() {
  bool has_kind = false;
  for (const Layer& layer : layers_) {
    for (const Feature& f : layer.features) {
      if (field_value(f, "kind")) {
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

  Layer regions;
  regions.id = "china.area";
  regions.name = "area";
  regions.visible = true;
  Layer lines;
  lines.id = "china.line";
  lines.name = "line";
  lines.visible = true;
  Layer points;
  points.id = "china.point";
  points.name = "point";
  points.visible = true;
  Layer texts;
  texts.id = "china.text";
  texts.name = "text";
  texts.visible = true;

  for (Layer& layer : layers_) {
    for (Feature& f : layer.features) {
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

  std::vector<Layer> next;
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
  layers_ = std::move(next);
  active_layer_id_ = layers_.front().id;
}

void MapScene::add_sample_features(Layer* layer, const std::string& tag) {
  if (!layer) {
    return;
  }
  Feature road;
  road.id = next_feature_id();
  road.kind = GeomKind::kLine;
  road.points = {{80, 220}, {220, 140}, {420, 280}, {620, 200}};
  road.fields = {{"name", tag + " road"}, {"type", "line"}};
  layer->features.push_back(std::move(road));

  Feature parcel;
  parcel.id = next_feature_id();
  parcel.kind = GeomKind::kPolygon;
  parcel.points = {{160, 300}, {280, 300}, {280, 400}, {160, 400}, {160, 300}};
  parcel.fields = {{"name", tag + " parcel"}, {"type", "polygon"}};
  layer->features.push_back(std::move(parcel));

  Feature node;
  node.id = next_feature_id();
  node.kind = GeomKind::kPoint;
  node.points = {{320, 180}};
  node.fields = {{"name", tag + " node"}, {"type", "point"}};
  layer->features.push_back(std::move(node));
}

std::vector<MapScene::LayerDesc> MapScene::layer_descs() const {
  std::vector<LayerDesc> out;
  out.reserve(layers_.size());
  for (const Layer& layer : layers_) {
    LayerDesc d;
    d.id = layer.id;
    d.name = layer.name;
    d.visible = layer.visible;
    d.active = (layer.id == active_layer_id_);
    out.push_back(std::move(d));
  }
  return out;
}

size_t MapScene::feature_count() const {
  size_t n = 0;
  for (const Layer& layer : layers_) {
    n += layer.features.size();
  }
  return n;
}

bool MapScene::create_layer(const std::string& name,
                            const std::string& /*geometry_type*/) {
  if (name.empty()) {
    return false;
  }
  std::string id = name;
  int suffix = 1;
  while (find_layer(id)) {
    id = name + "_" + std::to_string(suffix++);
  }
  Layer layer;
  layer.id = id;
  layer.name = name;
  layer.visible = true;
  active_layer_id_ = id;
  layers_.push_back(std::move(layer));
  return true;
}

bool MapScene::remove_layer(const std::string& id) {
  const auto it =
      std::find_if(layers_.begin(), layers_.end(),
                   [&](const Layer& l) { return l.id == id; });
  if (it == layers_.end()) {
    return false;
  }
  layers_.erase(it);
  if (active_layer_id_ == id) {
    active_layer_id_ = layers_.empty() ? std::string() : layers_.front().id;
  }
  selected_id_ = {};
  return true;
}

bool MapScene::set_layer_visible(const std::string& id, bool visible) {
  Layer* layer = find_layer(id);
  if (!layer) {
    return false;
  }
  layer->visible = visible;
  return true;
}

bool MapScene::select_layer(const std::string& id) {
  if (!find_layer(id)) {
    return false;
  }
  active_layer_id_ = id;
  return true;
}

bool MapScene::move_layer(const std::string& id, int delta) {
  if (delta == 0 || layers_.size() < 2) {
    return false;
  }
  int index = -1;
  for (size_t i = 0; i < layers_.size(); ++i) {
    if (layers_[i].id == id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return false;
  }
  const int target = index + delta;
  if (target < 0 || target >= static_cast<int>(layers_.size())) {
    return false;
  }
  std::swap(layers_[static_cast<size_t>(index)],
            layers_[static_cast<size_t>(target)]);
  return true;
}

content::FeatureId MapScene::append_from_draft(const tool::Draft& draft,
                                               const char* tool_id) {
  ensure_active_layer();
  Layer* layer = find_layer(active_layer_id_);
  if (!layer || draft.points.empty()) {
    return {};
  }
  Feature f;
  f.id = next_feature_id();
  f.kind = geom_from_tool(tool_id, draft.kind);
  f.points.reserve(draft.points.size());
  for (const tool::DraftPoint& p : draft.points) {
    double mx = 0;
    double my = 0;
    view_to_map(p.x_px, p.y_px, &mx, &my);
    f.points.push_back({mx, my});
  }
  if (f.kind == GeomKind::kPolygon && f.points.size() >= 3) {
    if (f.points.front().x != f.points.back().x ||
        f.points.front().y != f.points.back().y) {
      f.points.push_back(f.points.front());
    }
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(next_id_ - 1));
  f.fields = {{"name", std::string(layer->name) + "-" + buf},
              {"type", f.kind == GeomKind::kLine
                           ? "line"
                           : (f.kind == GeomKind::kPolygon ? "polygon"
                                                           : "point")}};
  layer->features.push_back(std::move(f));
  return layer->features.back().id;
}

const MapScene::Feature* MapScene::hit_test(int view_x, int view_y, int view_w,
                                           int view_h) {
  (void)view_w;
  (void)view_h;
  clear_selection();
  double mx = 0;
  double my = 0;
  view_to_map(view_x, view_y, &mx, &my);
  constexpr double kTol = 12.0;
  const double tol_map = kTol / (scale_ > 1e-9 ? scale_ : 1.0);
  const double tol2 = tol_map * tol_map;
  Feature* best = nullptr;
  double best_d = tol2;
  for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
    if (!it->visible) {
      continue;
    }
    for (auto fit = it->features.rbegin(); fit != it->features.rend(); ++fit) {
      if (fit->points.empty()) {
        continue;
      }
      if (fit->kind == GeomKind::kPoint || fit->kind == GeomKind::kText) {
        const double d = dist2(mx, my, fit->points[0].x, fit->points[0].y);
        if (d <= best_d) {
          best_d = d;
          best = &(*fit);
        }
      } else {
        for (const Vertex& p : fit->points) {
          const double d = dist2(mx, my, p.x, p.y);
          if (d <= best_d) {
            best_d = d;
            best = &(*fit);
          }
        }
      }
    }
  }
  if (best) {
    best->selected = true;
    selected_id_ = best->id;
  }
  return best;
}

bool MapScene::select_feature(const content::FeatureId& id) {
  clear_selection();
  Feature* f = find_feature(id);
  if (!f) {
    return false;
  }
  f->selected = true;
  selected_id_ = id;
  return true;
}

void MapScene::clear_selection() {
  for (Layer& layer : layers_) {
    for (Feature& f : layer.features) {
      f.selected = false;
    }
  }
  selected_id_ = {};
}

const MapScene::Feature* MapScene::selected_feature() const {
  for (const Layer& layer : layers_) {
    for (const Feature& f : layer.features) {
      if (f.id.len == selected_id_.len &&
          std::memcmp(f.id.bytes, selected_id_.bytes, f.id.len) == 0) {
        return &f;
      }
    }
  }
  return nullptr;
}

void MapScene::apply_pan(int dx_px, int dy_px) {
  pan_x_ += static_cast<double>(dx_px);
  pan_y_ += static_cast<double>(dy_px);
}

void MapScene::apply_zoom_at(int view_x, int view_y, double factor) {
  tool::zoom_at_client_point(&pan_x_, &pan_y_, &scale_, view_x, view_y, factor);
}

void MapScene::apply_pinch(int view_x, int view_y, double scale) {
  const int32_t wheel = tool::scale_to_wheel_delta(scale);
  if (wheel == 0) {
    return;
  }
  apply_zoom_at(view_x, view_y, tool::wheel_zoom_factor(wheel));
}

content::Extent2 MapScene::world_extent() const {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  if (!compute_extent(&minx, &miny, &maxx, &maxy)) {
    return kChinaLonLatExtent;
  }
  // Stored as lon / -lat.
  return content::Extent2{minx, -maxy, maxx, -miny};
}

content::Extent2 MapScene::view_world_extent(int view_w, int view_h) const {
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  double x0 = 0;
  double y0 = 0;
  double x1 = 0;
  double y1 = 0;
  view_to_map(0, 0, &x0, &y0);
  view_to_map(view_w, view_h, &x1, &y1);
  const double xmin = (std::min)(x0, x1);
  const double xmax = (std::max)(x0, x1);
  const double lat0 = -y0;
  const double lat1 = -y1;
  return content::Extent2{xmin, (std::min)(lat0, lat1), xmax,
                          (std::max)(lat0, lat1)};
}

void MapScene::apply_world_extent(const content::Extent2& e, int view_w,
                                 int view_h) {
  if (!extent_nonempty(e)) {
    return;
  }
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  const double minx = e.xmin;
  const double maxx = e.xmax;
  const double miny = -e.ymax;
  const double maxy = -e.ymin;
  const double dx = (std::max)(maxx - minx, 1e-9);
  const double dy = (std::max)(maxy - miny, 1e-9);
  const double sx = static_cast<double>(view_w) / dx;
  const double sy = static_cast<double>(view_h) / dy;
  scale_ = (std::min)(sx, sy);
  if (scale_ <= 0.0) {
    scale_ = 1.0;
  }
  const double cx = 0.5 * (minx + maxx);
  const double cy = 0.5 * (miny + maxy);
  pan_x_ = 0.5 * view_w - cx * scale_;
  pan_y_ = 0.5 * view_h - cy * scale_;
}

void MapScene::export_land_rings(
    std::vector<gis::LonLatRing>* out) const {
  if (!out) {
    return;
  }
  out->clear();
  auto append_layer = [&](const Layer& layer) {
    for (const Feature& f : layer.features) {
      if (f.kind != GeomKind::kPolygon || f.points.size() < 3) {
        continue;
      }
      gis::LonLatRing ring;
      ring.x.reserve(f.points.size());
      ring.y.reserve(f.points.size());
      for (const Vertex& p : f.points) {
        ring.x.push_back(p.x);
        ring.y.push_back(-p.y);  // undo map-space Y flip
      }
      out->push_back(std::move(ring));
    }
  };
  // Prefer `area` so DEM coastline matches the 2D prefecture fill.
  bool used_area = false;
  for (const Layer& layer : layers_) {
    if (!layer.visible) {
      continue;
    }
    if (layer.name == "area") {
      append_layer(layer);
      used_area = true;
    }
  }
  if (used_area && !out->empty()) {
    return;
  }
  for (const Layer& layer : layers_) {
    if (!layer.visible) {
      continue;
    }
    if (layer.name == "line" || layer.name == "point" ||
        layer.name == "text") {
      continue;
    }
    append_layer(layer);
  }
}

void MapScene::fit_extent(int view_w, int view_h) {
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  // China prefecture packs: Natural Earth rivers that only touch the loose
  // China bbox keep foreign stubs (Siberia / Central Asia), and area layers
  // include South China Sea vertices near ~4N. Framing on all vertices zooms
  // out so rivers appear to "spill" past provincial land. Match leftover's
  // mainland envelope instead.
  if (has_china_extent()) {
    apply_world_extent(kChinaLonLatExtent, view_w, view_h);
    return;
  }

  // Prefer land polygons when present so line/point outliers do not dominate.
  bool have = false;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  auto accumulate = [&](GeomKind only_kind, bool filter_kind) {
    have = false;
    for (const Layer& layer : layers_) {
      for (const Feature& f : layer.features) {
        if (filter_kind && f.kind != only_kind) {
          continue;
        }
        for (const Vertex& p : f.points) {
          if (!have) {
            minx = maxx = p.x;
            miny = maxy = p.y;
            have = true;
          } else {
            minx = std::min(minx, p.x);
            miny = std::min(miny, p.y);
            maxx = std::max(maxx, p.x);
            maxy = std::max(maxy, p.y);
          }
        }
      }
    }
  };
  accumulate(GeomKind::kPolygon, true);
  if (!have) {
    accumulate(GeomKind::kPolygon, false);
  }
  if (!have) {
    return;
  }
  const double dx = std::max(maxx - minx, 1.0);
  const double dy = std::max(maxy - miny, 1.0);
  const double pad = 0.08;
  const double sx = static_cast<double>(view_w) * (1.0 - 2.0 * pad) / dx;
  const double sy = static_cast<double>(view_h) * (1.0 - 2.0 * pad) / dy;
  scale_ = std::min(sx, sy);
  if (scale_ <= 0.0) {
    scale_ = 1.0;
  }
  const double cx = 0.5 * (minx + maxx);
  const double cy = 0.5 * (miny + maxy);
  pan_x_ = 0.5 * view_w - cx * scale_;
  pan_y_ = 0.5 * view_h - cy * scale_;
}

void MapScene::paint(HDC hdc, int width_px, int height_px) const {
  paint(hdc, width_px, height_px, true);
}

void MapScene::paint(HDC hdc, int width_px, int height_px,
                     bool fill_background) const {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }

  // Ocean canvas (Baidu-like). Callers may have filled a dark placeholder.
  if (fill_background) {
    HBRUSH bg = CreateSolidBrush(map_scene_map_bg_color());
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  // Reuse a few GDI objects for the whole frame. Creating Pen/Brush/Font per
  // feature leaked when early-continue skipped DeleteObject, and exhausted the
  // per-process GDI quota (~10k) on china_city (~1.4k features × 30 Hz).
  // Font heights are DIP → device px so 150%/200% displays stay readable.
  const int dpi = std::max(96, GetDeviceCaps(hdc, LOGPIXELSY));
  auto dip_px = [dpi](int px96) { return -MulDiv(px96, dpi, 96); };
  HPEN pens[4] = {};
  pens[0] =
      CreatePen(PS_SOLID, 1, map_scene_admin_stroke_color());  // polygon outline
  pens[1] = CreatePen(PS_SOLID, 2, map_scene_river_color());   // river
  pens[2] = CreatePen(PS_SOLID, 2, RGB(220, 210, 190));         // other line
  pens[3] = CreatePen(PS_SOLID, 3, RGB(200, 120, 40));         // selected
  HBRUSH land_brush = CreateSolidBrush(map_scene_area_fill_color(nullptr, 0));
  HBRUSH point_brush = CreateSolidBrush(map_scene_point_fill_color());
  HBRUSH selected_brush = CreateSolidBrush(RGB(255, 200, 80));
  HFONT fonts[3] = {};
  fonts[0] = CreateFontW(dip_px(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  fonts[1] = CreateFontW(dip_px(20), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  fonts[2] = CreateFontW(dip_px(28), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  HFONT status_font =
      CreateFontW(dip_px(14), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                  L"Microsoft YaHei UI");
  HGDIOBJ stock_font = GetStockObject(DEFAULT_GUI_FONT);
  HGDIOBJ old_pen = SelectObject(hdc, pens[0] ? pens[0] : GetStockObject(BLACK_PEN));
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  HGDIOBJ old_font = SelectObject(hdc, fonts[0] ? fonts[0] : stock_font);
  SetBkMode(hdc, TRANSPARENT);

  auto draw_label = [&](int vx, int vy, const std::string& name, COLORREF color,
                        int font_idx, int dx, int dy) {
    if (name.empty()) {
      return;
    }
    // Skip labels that are far outside the viewport (cheap cull).
    if (vx + dx < -80 || vy + dy < -40 || vx + dx > width_px + 80 ||
        vy + dy > height_px + 40) {
      return;
    }
    const std::wstring w = gis::datasource::ogr_bytes_to_wide(name);
    if (w.empty()) {
      return;
    }
    HFONT font = fonts[font_idx < 0 ? 0 : (font_idx > 2 ? 2 : font_idx)];
    SelectObject(hdc, font ? font : stock_font);
    const int x = vx + dx;
    const int y = vy + dy;
    const int n = static_cast<int>(w.size());
    // Light halo for CJK on pastel fills.
    SetTextColor(hdc, RGB(255, 255, 255));
    const int halo[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                            {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (const auto& d : halo) {
      TextOutW(hdc, x + d[0], y + d[1], w.c_str(), n);
    }
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, w.c_str(), n);
  };

  // After fit_extent on China, scale_ is typically ~8–15; avoid drowning the
  // frame in 400+ city labels at country view.
  const bool draw_dense_text = scale_ >= 18.0;
  bool has_text_features = false;
  for (const Layer& layer : layers_) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (f.kind == GeomKind::kText) {
        has_text_features = true;
        break;
      }
    }
    if (has_text_features) {
      break;
    }
  }
  constexpr size_t kMaxPolyPts = 2048;
  const GeomKind order[] = {GeomKind::kPolygon, GeomKind::kLine,
                            GeomKind::kPoint, GeomKind::kText};
  for (GeomKind pass : order) {
    for (const Layer& layer : layers_) {
      if (!layer.visible) {
        continue;
      }
      for (const Feature& f : layer.features) {
        if (f.kind != pass || f.points.empty()) {
          continue;
        }

        if (f.kind == GeomKind::kText) {
          const char* cls = field_value(f, "class");
          if (!draw_dense_text && cls &&
              std::strcmp(cls, "region_label") == 0) {
            continue;
          }
          int vx = 0;
          int vy = 0;
          map_to_view(f.points[0].x, f.points[0].y, &vx, &vy);
          int font_idx = 0;
          COLORREF ink = RGB(20, 18, 14);
          if (cls && std::strcmp(cls, "title") == 0) {
            font_idx = 2;
            ink = RGB(12, 10, 8);
          } else if (cls && std::strcmp(cls, "region_label") == 0) {
            font_idx = 1;
          } else if (cls && std::strcmp(cls, "river_label") == 0) {
            ink = RGB(8, 36, 72);
          }
          if (f.selected) {
            ink = RGB(200, 120, 40);
          }
          draw_label(vx, vy, feature_display_name(f), ink, font_idx, -20, -8);
          continue;
        }

        if (f.kind == GeomKind::kPoint) {
          int vx = 0;
          int vy = 0;
          map_to_view(f.points[0].x, f.points[0].y, &vx, &vy);
          if (vx < -20 || vy < -20 || vx > width_px + 20 ||
              vy > height_px + 20) {
            continue;
          }
          // Thin Baidu-like POI: white halo + soft fill (scale-aware radius).
          const int r = scale_ < 18.0 ? 1 : 2;
          const int outer = r + 1;
          HBRUSH ring = CreateSolidBrush(RGB(255, 255, 255));
          SelectObject(hdc, ring);
          SelectObject(hdc, GetStockObject(NULL_PEN));
          Ellipse(hdc, vx - outer, vy - outer, vx + outer + 1, vy + outer + 1);
          DeleteObject(ring);
          SelectObject(hdc, f.selected ? selected_brush : point_brush);
          SelectObject(hdc, f.selected ? pens[3] : pens[0]);
          Ellipse(hdc, vx - r, vy - r, vx + r + 1, vy + r + 1);
          SelectObject(hdc, GetStockObject(NULL_BRUSH));
          if (draw_dense_text && !has_text_features) {
            draw_label(vx, vy, feature_display_name(f), RGB(40, 32, 20), 0, 7,
                       -8);
          }
          continue;
        }

        if (f.kind == GeomKind::kLine) {
          const char* cls = field_value(f, "class");
          const char* kind = field_value(f, "kind");
          HPEN pen = pens[2];
          if (f.selected) {
            pen = pens[3];
          } else if ((cls && std::strcmp(cls, "river") == 0) ||
                     (kind && (std::strcmp(kind, "river") == 0 ||
                               std::strcmp(kind, "water") == 0))) {
            pen = pens[1];
          }
          SelectObject(hdc, pen ? pen : GetStockObject(BLACK_PEN));
          bool first = true;
          for (const Vertex& p : f.points) {
            int vx = 0;
            int vy = 0;
            map_to_view(p.x, p.y, &vx, &vy);
            if (first) {
              MoveToEx(hdc, vx, vy, nullptr);
              first = false;
            } else {
              LineTo(hdc, vx, vy);
            }
          }
          continue;
        }

        // Polygon: fill + outline. Cap vertex count for GDI Polygon safety.
        std::vector<POINT> pts;
        pts.reserve(std::min(f.points.size(), kMaxPolyPts));
        const size_t step =
            f.points.size() > kMaxPolyPts
                ? (f.points.size() + kMaxPolyPts - 1) / kMaxPolyPts
                : 1;
        for (size_t i = 0; i < f.points.size(); i += step) {
          int vx = 0;
          int vy = 0;
          map_to_view(f.points[i].x, f.points[i].y, &vx, &vy);
          pts.push_back({vx, vy});
        }
        if (pts.size() >= 3) {
          // Do NOT CreatePen/Brush per polygon — china_city has ~476 areas and
          // the present timer repaints ~30 Hz; per-feature GDI creates exhaust
          // the process quota and crash the host even when DeleteObject is called.
          SelectObject(hdc, f.selected ? pens[3] : pens[0]);
          HBRUSH fill = f.selected ? selected_brush : land_brush;
          SelectObject(hdc, fill ? fill : GetStockObject(LTGRAY_BRUSH));
          Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
          SelectObject(hdc, GetStockObject(NULL_BRUSH));
          Polyline(hdc, pts.data(), static_cast<int>(pts.size()));
          // Region name only when there is no dedicated text layer (china_plp
          // already has kind=label points). Avoid stacking the same CJK name.
          if (!has_text_features) {
            double mx = 0;
            double my = 0;
            label_anchor(f, &mx, &my);
            int lvx = 0;
            int lvy = 0;
            map_to_view(mx, my, &lvx, &lvy);
            draw_label(lvx, lvy, feature_display_name(f), RGB(32, 28, 22), 0,
                       -12, -6);
          }
        }
      }
    }
  }

  SelectObject(hdc, old_font);
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  for (HFONT font : fonts) {
    if (font) {
      DeleteObject(font);
    }
  }
  if (land_brush) {
    DeleteObject(land_brush);
  }
  if (point_brush) {
    DeleteObject(point_brush);
  }
  if (selected_brush) {
    DeleteObject(selected_brush);
  }
  for (HPEN pen : pens) {
    if (pen) {
      DeleteObject(pen);
    }
  }

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(24, 28, 32));
  SelectObject(hdc, status_font ? status_font : stock_font);
  wchar_t line[160];
  swprintf_s(line, L"Layers %zu  Features %zu  scale %.4g%s", layers_.size(),
             feature_count(), scale_, last_open_was_ogr_ ? L"  OGR" : L"");
  TextOutW(hdc, 12, height_px > 48 ? height_px - 36 : 12, line, lstrlenW(line));
  if (status_font) {
    SelectObject(hdc, stock_font);
    DeleteObject(status_font);
  }
}

void MapScene::paint_labels_projected(
    HDC hdc, int width_px, int height_px,
    const std::function<void(double lon, double lat, int* sx, int* sy)>&
        project) const {
  if (!hdc || width_px <= 0 || height_px <= 0 || !project) {
    return;
  }
  HFONT font = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  HGDIOBJ old_font =
      SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);

  auto draw_label = [&](int vx, int vy, const std::string& name) {
    if (name.empty()) {
      return;
    }
    if (vx < -80 || vy < -40 || vx > width_px + 80 || vy > height_px + 40) {
      return;
    }
    const std::wstring w = gis::datasource::ogr_bytes_to_wide(name);
    if (w.empty()) {
      return;
    }
    const int n = static_cast<int>(w.size());
    SetTextColor(hdc, RGB(20, 24, 32));
    const int halo[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                            {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (const auto& d : halo) {
      TextOutW(hdc, vx + d[0], vy + d[1], w.c_str(), n);
    }
    SetTextColor(hdc, RGB(245, 248, 252));
    TextOutW(hdc, vx, vy, w.c_str(), n);
  };

  constexpr size_t kMaxLabels = 48;
  size_t drawn = 0;
  bool has_text = false;
  for (const Layer& layer : layers_) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (f.kind == GeomKind::kText) {
        has_text = true;
        break;
      }
    }
    if (has_text) {
      break;
    }
  }

  auto emit = [&](const Feature& f) {
    if (drawn >= kMaxLabels) {
      return;
    }
    const std::string name = feature_display_name(f);
    if (name.empty()) {
      return;
    }
    double mx = 0;
    double my = 0;
    label_anchor(f, &mx, &my);
    // Map space Y is -lat.
    const double lon = mx;
    const double lat = -my;
    int sx = 0;
    int sy = 0;
    project(lon, lat, &sx, &sy);
    draw_label(sx, sy, name);
    ++drawn;
  };

  if (has_text) {
    for (const Layer& layer : layers_) {
      if (!layer.visible || drawn >= kMaxLabels) {
        continue;
      }
      for (const Feature& f : layer.features) {
        if (f.kind == GeomKind::kText && !f.points.empty()) {
          emit(f);
        }
      }
    }
  } else {
    // china_plp-style: region centroids carry the place name.
    for (const Layer& layer : layers_) {
      if (!layer.visible || drawn >= kMaxLabels) {
        continue;
      }
      for (const Feature& f : layer.features) {
        if (f.kind == GeomKind::kPolygon && f.points.size() >= 3) {
          emit(f);
        }
      }
    }
  }

  SelectObject(hdc, old_font);
  if (font) {
    DeleteObject(font);
  }
}

void MapScene::fill_feature_info_fields(
    const Feature& f,
    std::vector<std::pair<std::string, std::string>>* out) const {
  if (!out) {
    return;
  }
  out->clear();
  for (const Field& field : f.fields) {
    out->push_back({field.name, field.value});
  }
  out->push_back({"geom", f.kind == GeomKind::kLine
                              ? "line"
                              : (f.kind == GeomKind::kPolygon
                                     ? "polygon"
                                     : (f.kind == GeomKind::kText ? "text"
                                                                  : "point"))});
  out->push_back({"vertices", std::to_string(f.points.size())});
}

void MapScene::fill_attribute_rows(std::vector<std::string>* columns,
                                   std::vector<std::vector<std::string>>* rows,
                                   std::vector<std::string>* tokens) const {
  if (!columns || !rows || !tokens) {
    return;
  }
  columns->assign({"FID", "Name", "Type", "Layer"});
  rows->clear();
  tokens->clear();
  for (const Layer& layer : layers_) {
    for (const Feature& f : layer.features) {
      std::string name =
          f.fields.empty() ? feature_token(f.id) : f.fields[0].value;
      std::string type = f.kind == GeomKind::kLine
                             ? "line"
                             : (f.kind == GeomKind::kPolygon
                                    ? "region"
                                    : (f.kind == GeomKind::kText ? "text"
                                                                : "point"));
      for (const Field& field : f.fields) {
        if (field.name == "kind" || field.name == "type") {
          type = field.value;
        }
        if (field.name == "name" && !field.value.empty()) {
          name = field.value;
        }
        if (field.name == "anno" && !field.value.empty()) {
          name = field.value;
        }
      }
      rows->push_back({feature_token(f.id), name, type, layer.name});
      tokens->push_back(feature_token(f.id));
    }
  }
}

std::string MapScene::feature_token(const content::FeatureId& id) {
  return content::encode_feature_token(id);
}

content::FeatureId MapScene::feature_id_from_token(const std::string& token) {
  return content::decode_feature_token(token);
}

bool MapScene::update_feature_field(const std::string& token,
                                    const std::string& field,
                                    const std::string& value) {
  Feature* f = find_feature(feature_id_from_token(token));
  if (!f) {
    return false;
  }
  return content::apply_named_field(&f->fields, field, value);
}

MapScene::Layer* MapScene::find_layer(const std::string& id) {
  for (Layer& layer : layers_) {
    if (layer.id == id) {
      return &layer;
    }
  }
  return nullptr;
}

const MapScene::Layer* MapScene::find_layer(const std::string& id) const {
  for (const Layer& layer : layers_) {
    if (layer.id == id) {
      return &layer;
    }
  }
  return nullptr;
}

MapScene::Feature* MapScene::find_feature(const content::FeatureId& id) {
  for (Layer& layer : layers_) {
    for (Feature& f : layer.features) {
      if (f.id.len == id.len &&
          std::memcmp(f.id.bytes, id.bytes, id.len) == 0) {
        return &f;
      }
    }
  }
  return nullptr;
}

content::FeatureId MapScene::next_feature_id() {
  content::FeatureId id{};
  id.len = 4;
  const uint32_t v = next_id_++;
  id.bytes[0] = static_cast<uint8_t>(v & 0xff);
  id.bytes[1] = static_cast<uint8_t>((v >> 8) & 0xff);
  id.bytes[2] = static_cast<uint8_t>((v >> 16) & 0xff);
  id.bytes[3] = static_cast<uint8_t>((v >> 24) & 0xff);
  return id;
}

void MapScene::map_to_view(double mx, double my, int* vx, int* vy) const {
  // Keep GDI points inside a safe 16-bit-ish range. Extreme pan/zoom or bad
  // vertices otherwise overflow int and can AV inside Polygon/LineTo.
  constexpr double kLo = -30000.0;
  constexpr double kHi = 30000.0;
  if (vx) {
    const double x = mx * scale_ + pan_x_;
    *vx = static_cast<int>(
        std::lround(x < kLo ? kLo : (x > kHi ? kHi : x)));
  }
  if (vy) {
    const double y = my * scale_ + pan_y_;
    *vy = static_cast<int>(
        std::lround(y < kLo ? kLo : (y > kHi ? kHi : y)));
  }
}

void MapScene::view_to_map(int vx, int vy, double* mx, double* my) const {
  if (mx) {
    *mx = (static_cast<double>(vx) - pan_x_) / scale_;
  }
  if (my) {
    *my = (static_cast<double>(vy) - pan_y_) / scale_;
  }
}

void MapScene::ensure_active_layer() {
  if (find_layer(active_layer_id_)) {
    return;
  }
  if (layers_.empty()) {
    seed_default();
    return;
  }
  active_layer_id_ = layers_.front().id;
}

}  // namespace app

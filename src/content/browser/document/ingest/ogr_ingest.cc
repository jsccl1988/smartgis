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

#include "content/browser/document/ingest/seed_paths.h"
#include "gdal_priv.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "gis/datasource/ogr/ogr_text_encoding.h"
#include "gis/datasource/pipeline/feature_load_pipeline.h"
#include "vista/terrain/process/land_mask.h"
#include "ogrsf_frmts.h"
#include "base/process/switches.h"

namespace content {
namespace detail {
namespace {

constexpr size_t kMaxFeaturesPerLayer = 8000;

void append_ring(OGRLineString* ring, std::vector<Vertex>* out) {
  if (!ring || !out) {
    return;
  }
  const int n = ring->getNumPoints();
  if (n <= 0) {
    return;
  }
  // Cap ring density so GDI Polygon / paint stays bounded on prefecture packs.
  // Prefer denser coasts/islands at china overview (was 2048 → blocky Taiwan).
  constexpr int kMaxRingPoints = 4096;
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

// Keep contiguous runs inside a China lon/lat bbox (map space = lon / -lat).
// Foreign Natural Earth stems that only graze the box are dropped. When a
// mainland river briefly exits the box (lake / mouth), bridge runs by copying
// the original outside vertices — never invent a straight chord between runs
// (that drew diagonal blue artifacts across the china showcase).
void clip_line_to_bbox_run(std::vector<Vertex>* pts) {
  if (!pts || pts->size() < 2) {
    return;
  }
  constexpr double kMinLon = 73.0;
  constexpr double kMaxLon = 135.0;
  // Stored Y is -lat — mainland lat 18..54 becomes Y -54..-18.
  constexpr double kMinY = -54.0;
  constexpr double kMaxY = -18.0;
  auto inside = [&](const Vertex& p) {
    return p.x >= kMinLon && p.x <= kMaxLon && p.y >= kMinY && p.y <= kMaxY;
  };
  struct Run {
    size_t begin = 0;
    size_t len = 0;
  };
  std::vector<Run> runs;
  size_t i = 0;
  const size_t n = pts->size();
  size_t inside_total = 0;
  while (i < n) {
    while (i < n && !inside((*pts)[i])) {
      ++i;
    }
    const size_t begin = i;
    while (i < n && inside((*pts)[i])) {
      ++i;
    }
    const size_t len = i - begin;
    if (len >= 2) {
      runs.push_back({begin, len});
      inside_total += len;
    }
  }
  if (runs.empty()) {
    pts->clear();
    return;
  }
  // Drop lines that only graze the mainland box (NE river stubs that briefly
  // enter then scribble across ocean). Require ≥25% of samples inside.
  if (inside_total * 4 < n) {
    pts->clear();
    return;
  }
  if (runs.size() == 1 && runs[0].len == n) {
    return;
  }

  auto run_end = [](const Run& run) { return run.begin + run.len; };

  // Chain nearby in-bbox runs without copying outside gap vertices (those
  // became fake land diagonals). Prefer a single longest run when the gap
  // span is large — micro-bridges only for near mouth stubs.
  constexpr size_t kMaxBridgeVerts = 8;
  constexpr double kMaxBridgeDeg = 0.06;

  auto can_bridge = [&](const Run& a, const Run& b) {
    if (b.begin < run_end(a)) {
      return false;
    }
    const size_t gap = b.begin - run_end(a);
    if (gap > kMaxBridgeVerts) {
      return false;
    }
    const Vertex& pa = (*pts)[run_end(a) - 1];
    const Vertex& pb = (*pts)[b.begin];
    const double dx = pb.x - pa.x;
    const double dy = pb.y - pa.y;
    return dx * dx + dy * dy <= kMaxBridgeDeg * kMaxBridgeDeg;
  };

  size_t best_begin = 0;
  size_t best_end = 1;
  size_t best_score = runs[0].len;
  for (size_t start = 0; start < runs.size(); ++start) {
    size_t score = runs[start].len;
    size_t end = start + 1;
    while (end < runs.size() && can_bridge(runs[end - 1], runs[end])) {
      score += runs[end].len;
      ++end;
    }
    if (score > best_score) {
      best_score = score;
      best_begin = start;
      best_end = end;
    }
  }

  std::vector<Vertex> kept;
  kept.reserve(best_score);
  for (size_t r = best_begin; r < best_end; ++r) {
    kept.insert(kept.end(),
                pts->begin() + static_cast<std::ptrdiff_t>(runs[r].begin),
                pts->begin() +
                    static_cast<std::ptrdiff_t>(run_end(runs[r])));
  }
  // Drop remaining long edges (sparse NE / simplified stems) by keeping the
  // longest short-edge contiguous span — do not clear mid-pass.
  constexpr double kMaxEdgeDeg = 1.25;
  size_t seg_begin = 0;
  size_t best_seg_begin = 0;
  size_t best_seg_len = 0;
  auto flush_seg = [&](size_t end) {
    const size_t len = end - seg_begin;
    if (len > best_seg_len) {
      best_seg_len = len;
      best_seg_begin = seg_begin;
    }
  };
  for (size_t i = 1; i < kept.size(); ++i) {
    const double dx = kept[i].x - kept[i - 1].x;
    const double dy = kept[i].y - kept[i - 1].y;
    if (dx * dx + dy * dy > kMaxEdgeDeg * kMaxEdgeDeg) {
      flush_seg(i);
      seg_begin = i;
    }
  }
  flush_seg(kept.size());
  if (best_seg_len >= 2) {
    pts->assign(kept.begin() + static_cast<std::ptrdiff_t>(best_seg_begin),
                kept.begin() +
                    static_cast<std::ptrdiff_t>(best_seg_begin + best_seg_len));
  } else if (kept.size() >= 2) {
    *pts = std::move(kept);
  } else {
    pts->clear();
  }
}

void clip_china_city_line_to_mainland(std::vector<Vertex>* pts,
                                      const char* ogr_layer_name) {
  if (!pts || pts->size() < 2 || !ogr_layer_name) {
    return;
  }
  if (std::strcmp(ogr_layer_name, "line") != 0) {
    return;
  }
  clip_line_to_bbox_run(pts);
}

bool path_looks_like_china_city(const std::string& path) {
  std::string stem = path_stem(path);
  for (char& c : stem) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return stem.find("china_city") != std::string::npos;
}

// Drop china_city river/road stubs that sit in the lon/lat bbox but outside
// admin land polygons (the bbox includes Bohai / ECS / SCS so bbox-only clip
// still paints blue scribble over "ocean" background).
bool vertex_bbox_overlaps_any_ring_bbox(
    const MapFeature& f, const std::vector<vista::LonLatRing>& rings) {
  if (f.points.empty() || rings.empty()) {
    return false;
  }
  double min_x = f.points[0].x;
  double max_x = min_x;
  double min_y = f.points[0].y;
  double max_y = min_y;
  for (size_t i = 1; i < f.points.size(); ++i) {
    const Vertex& p = f.points[i];
    min_x = (std::min)(min_x, p.x);
    max_x = (std::max)(max_x, p.x);
    min_y = (std::min)(min_y, p.y);
    max_y = (std::max)(max_y, p.y);
  }
  for (const vista::LonLatRing& ring : rings) {
    if (!ring.has_bbox) {
      ring.prepare_bbox();
    }
    if (max_x < ring.minx || min_x > ring.maxx || max_y < ring.miny ||
        min_y > ring.maxy) {
      continue;
    }
    return true;
  }
  return false;
}

void clip_china_city_lines_to_land_polygons(LayerStore* store) {
  if (!store) {
    return;
  }
  // Full land-clip is O(lines×rings) and can freeze the UI thread for tens of
  // seconds (deferred China seed). Harness / deferred path sets
  // SKIP_CHINA_LAND_CLIP=1; sync showcase keeps the clip for ocean cleanup.
  if (const char* skip = base::switch_cstr("skip-china-land-clip")) {
    if (skip[0] == '1' && skip[1] == '\0') {
      // Intentional: deferred / harness path must not freeze the UI thread.
      return;
    }
  }
  std::vector<vista::LonLatRing> rings;
  for (const MapLayer& layer : store->layers()) {
    for (const MapFeature& f : layer.features) {
      if (f.kind != GeomKind::kPolygon || f.points.size() < 3) {
        continue;
      }
      vista::LonLatRing ring;
      ring.x.reserve(f.points.size());
      ring.y.reserve(f.points.size());
      for (const Vertex& p : f.points) {
        ring.x.push_back(p.x);
        // Map space stores -lat; land_mask even-odd is axis-agnostic.
        ring.y.push_back(p.y);
      }
      ring.prepare_bbox();
      rings.push_back(std::move(ring));
    }
  }
  if (rings.empty()) {
    return;
  }
  auto point_on_land = [&](const Vertex& p) {
    return vista::any_ring_contains(p.x, p.y, rings);
  };
  for (MapLayer& layer : store->layers()) {
    auto& feats = layer.features;
    feats.erase(
        std::remove_if(
            feats.begin(), feats.end(),
            [&](MapFeature& f) {
              if (f.kind != GeomKind::kLine || f.points.size() < 2) {
                return false;
              }
              if (!vertex_bbox_overlaps_any_ring_bbox(f, rings)) {
                return true;
              }
              // Keep on-land runs; drop ocean-only stubs. Require ≥25% of
              // samples on land (bbox-only still paints blue over Bohai/ECS).
              // Chain nearby runs so lake/mouth gaps do not chop Yangtze stems.
              struct Run {
                size_t begin = 0;
                size_t len = 0;
              };
              std::vector<Run> runs;
              size_t i = 0;
              const size_t n = f.points.size();
              size_t on_land_total = 0;
              while (i < n) {
                while (i < n && !point_on_land(f.points[i])) {
                  ++i;
                }
                const size_t begin = i;
                while (i < n && point_on_land(f.points[i])) {
                  ++i;
                }
                const size_t len = i - begin;
                if (len >= 2) {
                  runs.push_back({begin, len});
                  on_land_total += len;
                }
              }
              if (runs.empty()) {
                return true;
              }
              if (on_land_total * 4 < n) {
                return true;
              }
              auto run_end = [](const Run& run) { return run.begin + run.len; };
              constexpr size_t kMaxBridgeVerts = 8;
              constexpr double kMaxBridgeDeg = 0.06;
              auto can_bridge = [&](const Run& a, const Run& b) {
                if (b.begin < run_end(a)) {
                  return false;
                }
                const size_t gap = b.begin - run_end(a);
                if (gap > kMaxBridgeVerts) {
                  return false;
                }
                const Vertex& pa = f.points[run_end(a) - 1];
                const Vertex& pb = f.points[b.begin];
                const double dx = pb.x - pa.x;
                const double dy = pb.y - pa.y;
                return dx * dx + dy * dy <= kMaxBridgeDeg * kMaxBridgeDeg;
              };
              size_t best_begin = 0;
              size_t best_end = 1;
              size_t best_score = runs[0].len;
              for (size_t start = 0; start < runs.size(); ++start) {
                size_t score = runs[start].len;
                size_t end = start + 1;
                while (end < runs.size() &&
                       can_bridge(runs[end - 1], runs[end])) {
                  score += runs[end].len;
                  ++end;
                }
                if (score > best_score) {
                  best_score = score;
                  best_begin = start;
                  best_end = end;
                }
              }
              std::vector<Vertex> kept;
              kept.reserve(best_score);
              for (size_t r = best_begin; r < best_end; ++r) {
                kept.insert(
                    kept.end(),
                    f.points.begin() +
                        static_cast<std::ptrdiff_t>(runs[r].begin),
                    f.points.begin() +
                        static_cast<std::ptrdiff_t>(run_end(runs[r])));
              }
              constexpr double kMaxEdgeDeg = 1.25;
              size_t seg_begin = 0;
              size_t best_seg_begin = 0;
              size_t best_seg_len = 0;
              auto flush_seg = [&](size_t end) {
                const size_t len = end - seg_begin;
                if (len > best_seg_len) {
                  best_seg_len = len;
                  best_seg_begin = seg_begin;
                }
              };
              for (size_t i = 1; i < kept.size(); ++i) {
                const double dx = kept[i].x - kept[i - 1].x;
                const double dy = kept[i].y - kept[i - 1].y;
                if (dx * dx + dy * dy > kMaxEdgeDeg * kMaxEdgeDeg) {
                  flush_seg(i);
                  seg_begin = i;
                }
              }
              flush_seg(kept.size());
              if (best_seg_len >= 2) {
                f.points.assign(
                    kept.begin() +
                        static_cast<std::ptrdiff_t>(best_seg_begin),
                    kept.begin() + static_cast<std::ptrdiff_t>(
                                       best_seg_begin + best_seg_len));
              } else if (kept.size() >= 2) {
                f.points = std::move(kept);
              } else {
                return true;
              }
              return false;
            }),
        feats.end());
  }
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

void copy_ogr_fields(OGRFeature* ogr_feat, MapFeature* out) {
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

bool fill_polygon_feature(OGRPolygon* poly, MapFeature* out,
                          const char* ogr_layer_name) {
  if (!poly || !out) {
    return false;
  }
  out->kind = GeomKind::kPolygon;
  out->points.clear();
  out->selected = false;
  if (OGRLinearRing* ext = poly->getExteriorRing()) {
    append_ring(ext, &out->points);
  }
  apply_kind_override(out, ogr_layer_name);
  return out->points.size() >= 3;
}

bool fill_line_feature(OGRLineString* line, MapFeature* out,
                       const char* ogr_layer_name) {
  if (!line || !out) {
    return false;
  }
  out->kind = GeomKind::kLine;
  out->points.clear();
  out->selected = false;
  append_ring(line, &out->points);
  clip_china_city_line_to_mainland(&out->points, ogr_layer_name);
  apply_kind_override(out, ogr_layer_name);
  return out->points.size() >= 2;
}

// One MapFeature per drawable part. MultiPolygon / MultiLineString must
// expand every part  - keeping only the largest ring leaves Xinjiang/Qinghai
// (and island archipelagos) as white holes while rivers still draw through.
size_t features_from_ogr(OGRFeature* ogr_feat,
                         std::vector<MapFeature>* out,
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
    MapFeature f;
    copy_ogr_fields(ogr_feat, &f);
    auto* pt = geom->toPoint();
    f.kind = GeomKind::kPoint;
    f.points.push_back({pt->getX(), -pt->getY()});
    apply_kind_override(&f, ogr_layer_name);
    out->push_back(std::move(f));
    return 1;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    MapFeature f;
    copy_ogr_fields(ogr_feat, &f);
    if (!fill_line_feature(geom->toLineString(), &f, ogr_layer_name)) {
      return 0;
    }
    out->push_back(std::move(f));
    return 1;
  }
  if (flat == wkbPolygon) {
    MapFeature f;
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
    MapFeature f;
    copy_ogr_fields(ogr_feat, &f);
    auto* pt = multi->getGeometryRef(0)->toPoint();
    f.kind = GeomKind::kPoint;
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
      MapFeature f;
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
      MapFeature f;
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
  if (path_looks_like_china_city(path)) {
    clip_china_city_lines_to_land_polygons(store);
  }
  return true;
}

void split_layers_by_kind_field(LayerStore* store) {
  if (!store) {
    return;
  }
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
  store->replace_layers(std::move(next));
}

}  // namespace detail
}  // namespace content

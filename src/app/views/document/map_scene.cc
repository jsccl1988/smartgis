// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/document/map_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#include "app/views/camera/map_host_extent.h"

#include "content/public/feature_attrs.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/ogr/text/ogr_text_encoding.h"
#include "gis/present/style/style_document.h"
#include "gis/present/style/style_rules.h"
#include "gis/present/style/style_types.h"
#include "gis/vista/frame/frame.h"
#include "gis/vista/world/terrain/land_mask.h"

#include <mutex>

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

COLORREF argb_to_colorref(uint32_t argb) {
  return RGB(static_cast<int>((argb >> 16) & 0xFF),
             static_cast<int>((argb >> 8) & 0xFF),
             static_cast<int>(argb & 0xFF));
}

std::string argb_to_hex_rgb(uint32_t argb) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                static_cast<unsigned>((argb >> 16) & 0xFF),
                static_cast<unsigned>((argb >> 8) & 0xFF),
                static_cast<unsigned>(argb & 0xFF));
  return buf;
}

std::string colorref_to_hex(COLORREF c) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(c), GetGValue(c),
                GetBValue(c));
  return buf;
}

const gis::style::StyleDocument* embedded_carto_style_document() {
  static std::once_flag once;
  static std::shared_ptr<gis::style::StyleDocument> doc;
  std::call_once(once, [] {
    auto parsed = std::make_shared<gis::style::StyleDocument>();
    if (gis::style::parse_style_document(gis::vista::default_carto_style_json(),
                                         parsed.get())) {
      doc = std::move(parsed);
    }
  });
  return doc.get();
}

void try_load_accompanying_style(MapScene* scene, const std::string& path) {
  if (!scene || path.empty()) {
    return;
  }
  if (scene->load_style_path(path + ".style.json")) {
    return;
  }
  const size_t slash = path.find_last_of("/\\");
  const std::string dir =
      slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
  const std::string stem = path_stem(path);
  if (!stem.empty() && scene->load_style_path(dir + stem + ".style.json")) {
    return;
  }
  scene->load_style_path(dir + "china_city.style.json");
}

bool source_layer_for_feature(const MapScene& scene,
                              const content::FeatureId& id,
                              std::string* out) {
  if (!out) {
    return false;
  }
  for (const MapScene::Layer& layer : scene.layers()) {
    for (const MapScene::Feature& feature : layer.features) {
      if (std::memcmp(feature.id.bytes, id.bytes, sizeof(id.bytes)) == 0 &&
          feature.id.len == id.len) {
        *out = layer.name;
        return true;
      }
    }
  }
  return false;
}

void append_resolved_paint_rows(const gis::style::ResolvedPaint& paint,
                                MapScene::GeomKind kind,
                                std::vector<std::pair<std::string, std::string>>*
                                    out) {
  if (!out) {
    return;
  }
  out->push_back({"style layer id", paint.layer_id});
  switch (kind) {
    case MapScene::GeomKind::kPolygon:
      out->push_back({"fill-color", argb_to_hex_rgb(paint.fill_color)});
      out->push_back(
          {"fill-opacity", std::to_string(paint.fill_opacity)});
      out->push_back(
          {"note", "polygon outline uses fixed GDI admin stroke when fallback"});
      break;
    case MapScene::GeomKind::kLine:
      out->push_back({"line-color", argb_to_hex_rgb(paint.line_color)});
      out->push_back({"line-width", std::to_string(paint.line_width)});
      out->push_back(
          {"line-opacity", std::to_string(paint.line_opacity)});
      break;
    case MapScene::GeomKind::kPoint:
      out->push_back({"circle-color", argb_to_hex_rgb(paint.circle_color)});
      out->push_back(
          {"circle-radius", std::to_string(paint.circle_radius)});
      out->push_back(
          {"circle-opacity", std::to_string(paint.circle_opacity)});
      break;
    case MapScene::GeomKind::kText:
      out->push_back({"text-size", std::to_string(paint.text_size)});
      if (paint.text_halo_width > 0.f) {
        out->push_back(
            {"text-halo-color", argb_to_hex_rgb(paint.text_halo_color)});
        out->push_back(
            {"text-halo-width", std::to_string(paint.text_halo_width)});
      }
      break;
  }
}

// MapLibre-ish zoom from overlay scale (px per map unit / degree).
double zoom_from_scale(double scale) {
  const double z = 8.0 + std::log2(std::max(scale, 1e-3));
  if (z < 0.0) {
    return 0.0;
  }
  if (z > 22.0) {
    return 22.0;
  }
  return z;
}

std::vector<std::string> style_seed_relative_paths() {
  return {
      "china_city.style.json",
      "testing\\data\\china_city.style.json",
      "..\\testing\\data\\china_city.style.json",
      "..\\..\\testing\\data\\china_city.style.json",
  };
}

bool read_file_bytes(const std::string& path, std::string* out) {
  if (!out || path.empty()) {
    return false;
  }
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
    return false;
  }
  if (std::fseek(f, 0, SEEK_END) != 0) {
    std::fclose(f);
    return false;
  }
  const long len = std::ftell(f);
  if (len < 0) {
    std::fclose(f);
    return false;
  }
  if (std::fseek(f, 0, SEEK_SET) != 0) {
    std::fclose(f);
    return false;
  }
  out->assign(static_cast<size_t>(len), '\0');
  const size_t n = std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  return n == out->size();
}

}  // namespace

MapScene::MapScene() = default;

MapScene::~MapScene() = default;

void MapScene::clear() {
  layers_.clear();
  active_layer_id_.clear();
  selected_id_ = {};
  last_open_was_ogr_ = false;
}

void MapScene::set_style_document(
    std::shared_ptr<gis::style::StyleDocument> doc) {
  style_doc_ = std::move(doc);
}

void MapScene::clear_style_document() {
  style_doc_.reset();
}

bool MapScene::load_style_path(const std::string& path) {
  std::string json;
  if (!read_file_bytes(path, &json)) {
    return false;
  }
  auto doc = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(json, doc.get())) {
    return false;
  }
  style_doc_ = std::move(doc);
  return true;
}

void MapScene::set_basemap_provider(
    std::shared_ptr<gis::tile::TileProvider> provider) {
  basemap_ = std::move(provider);
}

void MapScene::clear_basemap_provider() {
  basemap_.reset();
}

bool MapScene::resolve_style_for_test(const std::string& source_layer,
                                     const gis::style::AttrMap& attrs,
                                     double zoom,
                                     gis::style::ResolvedPaint* out) const {
  if (!style_doc_ || !out) {
    return false;
  }
  return gis::style::resolve(*style_doc_, nullptr, attrs, zoom, source_layer,
                             out);
}

bool MapScene::style_colors_for_feature(const Layer& layer, const Feature& f,
                                       double scale, COLORREF* fill,
                                       COLORREF* stroke,
                                       int* stroke_width) const {
  if (!style_doc_ || !fill || !stroke || !stroke_width) {
    return false;
  }
  gis::style::AttrMap attrs;
  for (const Field& field : f.fields) {
    attrs[field.name] = field.value;
  }
  gis::style::ResolvedPaint paint;
  if (!gis::style::resolve(*style_doc_, nullptr, attrs, zoom_from_scale(scale),
                           layer.name, &paint)) {
    return false;
  }
  switch (f.kind) {
    case GeomKind::kPolygon:
      *fill = argb_to_colorref(paint.fill_color);
      *stroke = RGB(196, 190, 176);
      *stroke_width = 1;
      return true;
    case GeomKind::kLine:
      *stroke = argb_to_colorref(paint.line_color);
      *fill = *stroke;
      *stroke_width = std::max(1, static_cast<int>(std::lround(paint.line_width)));
      return true;
    case GeomKind::kPoint:
      *fill = argb_to_colorref(paint.circle_color);
      *stroke = *fill;
      *stroke_width = 1;
      return true;
    default:
      return false;
  }
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
    char exe_dir[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
      for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (exe_dir[i] == '\\' || exe_dir[i] == '/') {
          exe_dir[i + 1] = '\0';
          break;
        }
      }
      for (const std::string& rel : style_seed_relative_paths()) {
        if (load_style_path(std::string(exe_dir) + rel)) {
          break;
        }
      }
    }
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
    try_load_accompanying_style(this, path);
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

bool MapScene::write_path(const std::string& path) const {
  if (path.empty()) {
    return false;
  }
  const Layer* layer = find_layer(active_layer_id_);
  if (!layer || !layer->visible || layer->features.empty()) {
    layer = nullptr;
    for (const Layer& candidate : layers_) {
      if (candidate.visible && !candidate.features.empty()) {
        layer = &candidate;
        break;
      }
    }
  }
  if (!layer || layer->features.empty()) {
    return false;
  }

  const GeomKind dominant = layer->features.front().kind;
  OGRwkbGeometryType wkb = wkbUnknown;
  switch (dominant) {
    case GeomKind::kPoint:
    case GeomKind::kText:
      wkb = wkbPoint;
      break;
    case GeomKind::kLine:
      wkb = wkbLineString;
      break;
    case GeomKind::kPolygon:
      wkb = wkbPolygon;
      break;
  }
  if (wkb == wkbUnknown) {
    return false;
  }

  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  {
    GDALDataset* existing = static_cast<GDALDataset*>(GDALOpenEx(
        path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (existing) {
      GDALClose(existing);
      if (driver->Delete(path.c_str()) != CE_None) {
        DeleteFileA(path.c_str());
      }
    }
  }
  GDALDataset* ds = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
  if (!ds) {
    return false;
  }
  OGRLayer* ogr_layer = ds->CreateLayer(
      layer->name.empty() ? "layer" : layer->name.c_str(), nullptr, wkb,
      nullptr);
  if (!ogr_layer) {
    GDALClose(ds);
    return false;
  }

  std::vector<std::string> field_names;
  for (const Feature& f : layer->features) {
    if (f.kind != dominant) {
      continue;
    }
    for (const Field& field : f.fields) {
      if (field.name.empty()) {
        continue;
      }
      bool seen = false;
      for (const std::string& n : field_names) {
        if (n == field.name) {
          seen = true;
          break;
        }
      }
      if (!seen) {
        field_names.push_back(field.name);
      }
    }
  }
  for (const std::string& name : field_names) {
    OGRFieldDefn defn(name.c_str(), OFTString);
    if (ogr_layer->CreateField(&defn) != OGRERR_NONE) {
      GDALClose(ds);
      return false;
    }
  }

  size_t written = 0;
  for (const Feature& f : layer->features) {
    if (f.kind != dominant || f.points.empty()) {
      continue;
    }
    OGRFeatureUniquePtr feat(
        OGRFeature::CreateFeature(ogr_layer->GetLayerDefn()));
    if (!feat) {
      continue;
    }
    for (size_t fi = 0; fi < field_names.size(); ++fi) {
      const char* v = field_value(f, field_names[fi].c_str());
      if (v) {
        feat->SetField(static_cast<int>(fi), v);
      }
    }

    // Undo map-space Y flip (stored as lon / -lat) back to CRS84.
    if (dominant == GeomKind::kPoint || dominant == GeomKind::kText) {
      OGRPoint pt(f.points.front().x, -f.points.front().y);
      feat->SetGeometry(&pt);
    } else if (dominant == GeomKind::kLine) {
      if (f.points.size() < 2) {
        continue;
      }
      OGRLineString line;
      for (const Vertex& p : f.points) {
        line.addPoint(p.x, -p.y);
      }
      feat->SetGeometry(&line);
    } else if (dominant == GeomKind::kPolygon) {
      if (f.points.size() < 3) {
        continue;
      }
      OGRLinearRing ring;
      for (const Vertex& p : f.points) {
        ring.addPoint(p.x, -p.y);
      }
      if (ring.getNumPoints() >= 2) {
        const double x0 = ring.getX(0);
        const double y0 = ring.getY(0);
        const int last = ring.getNumPoints() - 1;
        if (x0 != ring.getX(last) || y0 != ring.getY(last)) {
          ring.addPoint(x0, y0);
        }
      }
      OGRPolygon poly;
      poly.addRing(&ring);
      feat->SetGeometry(&poly);
    }

    if (ogr_layer->CreateFeature(feat.get()) == OGRERR_NONE) {
      ++written;
    }
  }

  GDALClose(ds);
  return written > 0;
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

content::FeatureId MapScene::append_from_draft(
    const tool::Draft& draft, const char* tool_id,
    const std::function<void(int view_x, int view_y, double* map_x,
                             double* map_y)>& to_map) {
  // TODO(sp3): HWND-free draft→feature body belongs in
  // content::commit_draft_features once a shared Feature model is available.
  // Today Features are MapScene-local. |to_map| supplies the pixel transform.
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
    double mx = static_cast<double>(p.x_px);
    double my = static_cast<double>(p.y_px);
    if (to_map) {
      to_map(p.x_px, p.y_px, &mx, &my);
    }
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

content::FeatureId MapScene::move_selected_vertex(double map_x, double map_y,
                                                 double tol_map) {
  Feature* feature = find_feature(selected_id_);
  if (!feature || feature->points.empty()) {
    return {};
  }
  const double mx = map_x;
  const double my = map_y;
  const double tol2 = tol_map * tol_map;
  int best = -1;
  double best_d = tol2;
  for (size_t i = 0; i < feature->points.size(); ++i) {
    const double d = dist2(mx, my, feature->points[i].x, feature->points[i].y);
    if (d <= best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  if (best < 0) {
    return {};
  }
  feature->points[static_cast<size_t>(best)] = {mx, my};
  return feature->id;
}

bool MapScene::copy_feature_xy(
    const content::FeatureId& id,
    std::vector<std::pair<double, double>>* out) const {
  if (!out) {
    return false;
  }
  out->clear();
  for (const Layer& layer : layers_) {
    for (const Feature& feature : layer.features) {
      if (feature.id.len != id.len ||
          std::memcmp(feature.id.bytes, id.bytes, id.len) != 0) {
        continue;
      }
      out->reserve(feature.points.size());
      for (const Vertex& p : feature.points) {
        out->push_back({p.x, p.y});
      }
      return true;
    }
  }
  return false;
}

bool MapScene::add_triangle_layer(const std::string& name, const double* xyz,
                                 int point_count, const int* triangles,
                                 int triangle_count) {
  if (name.empty() || !xyz || point_count < 3 || !triangles ||
      triangle_count < 1) {
    return false;
  }
  if (!create_layer(name, "Polygon")) {
    return false;
  }
  Layer* layer = find_layer(active_layer_id_);
  if (!layer) {
    return false;
  }
  int added = 0;
  for (int t = 0; t < triangle_count; ++t) {
    const int a = triangles[t * 3];
    const int b = triangles[t * 3 + 1];
    const int c = triangles[t * 3 + 2];
    if (a < 0 || b < 0 || c < 0 || a >= point_count || b >= point_count ||
        c >= point_count) {
      continue;
    }
    Feature feature;
    feature.id = next_feature_id();
    feature.kind = GeomKind::kPolygon;
    const int idx[3] = {a, b, c};
    for (int k = 0; k < 3; ++k) {
      const int i = idx[k];
      feature.points.push_back({xyz[i * 3], xyz[i * 3 + 1]});
    }
    feature.points.push_back(feature.points.front());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", added + 1);
    feature.fields = {{"name", name + "-" + buf},
                      {"type", "polygon"},
                      {"z", std::to_string(xyz[a * 3 + 2])}};
    layer->features.push_back(std::move(feature));
    ++added;
  }
  if (added == 0) {
    const std::string empty_id = layer->id;
    remove_layer(empty_id);
    return false;
  }
  return true;
}

const MapScene::Feature* MapScene::hit_test(double map_x, double map_y,
                                             double tol_map) {
  clear_selection();
  const double mx = map_x;
  const double my = map_y;
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

namespace {

bool envelope_world(const std::vector<MapScene::Vertex>& points,
                    content::Extent2* out) {
  if (!out || points.empty()) {
    return false;
  }
  double minx = points[0].x;
  double maxx = minx;
  double miny = points[0].y;
  double maxy = miny;
  for (const MapScene::Vertex& p : points) {
    minx = std::min(minx, p.x);
    maxx = std::max(maxx, p.x);
    miny = std::min(miny, p.y);
    maxy = std::max(maxy, p.y);
  }
  // A point or axis-aligned stub still needs a nonempty frame.
  if (!(maxx > minx)) {
    minx -= 1e-4;
    maxx += 1e-4;
  }
  if (!(maxy > miny)) {
    miny -= 1e-4;
    maxy += 1e-4;
  }
  // Stored map Y is -lat.
  *out = content::Extent2{minx, -maxy, maxx, -miny};
  return extent_nonempty(*out);
}

}  // namespace

bool MapScene::active_layer_world_extent(content::Extent2* out) const {
  const Layer* layer = find_layer(active_layer_id_);
  if (!layer) {
    return false;
  }
  std::vector<Vertex> points;
  for (const Feature& feature : layer->features) {
    points.insert(points.end(), feature.points.begin(), feature.points.end());
  }
  return envelope_world(points, out);
}

bool MapScene::selection_world_extent(content::Extent2* out) const {
  const Feature* feature = selected_feature();
  if (!feature) {
    return false;
  }
  return envelope_world(feature->points, out);
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

bool MapScene::polygon_fit_box(double* min_x, double* min_y, double* max_x,
                               double* max_y) const {
  if (!min_x || !min_y || !max_x || !max_y) {
    return false;
  }
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
    return false;
  }
  *min_x = minx;
  *min_y = miny;
  *max_x = maxx;
  *max_y = maxy;
  return true;
}

void MapScene::fill_feature_info_fields(
    const Feature& f, std::vector<std::pair<std::string, std::string>>* out,
    const std::string& source_layer, double map_scale) const {
  if (!out) {
    return;
  }
  out->clear();

  std::string layer_name = source_layer;
  if (layer_name.empty()) {
    source_layer_for_feature(*this, f.id, &layer_name);
  }

  const gis::style::StyleDocument* doc = style_doc_.get();
  bool using_embedded_carto = false;
  if (!doc) {
    doc = embedded_carto_style_document();
    using_embedded_carto = (doc != nullptr);
  }

  out->push_back({"--- Style (ResolvedPaint) ---", ""});
  if (style_doc_) {
    const std::string label =
        style_doc_->name.empty() ? "(loaded Style JSON)" : style_doc_->name;
    out->push_back({"style source", label});
    out->push_back({"paint resolver", "MapScene StyleDocument"});
  } else if (using_embedded_carto) {
    out->push_back(
        {"style source",
         "(no Style JSON on MapScene — showing embedded default_carto)"});
    out->push_back(
        {"paint resolver",
         "default_carto_style_json (Map2dPresenter parity; load .style.json "
         "to override)"});
  } else {
    out->push_back({"style source", "(style parse unavailable)"});
  }
  if (!layer_name.empty()) {
    out->push_back({"source-layer", layer_name});
  }
  out->push_back({"zoom", std::to_string(zoom_from_scale(map_scale))});

  gis::style::AttrMap attrs;
  for (const Field& field : f.fields) {
    attrs[field.name] = field.value;
  }
  gis::style::ResolvedPaint paint;
  const bool resolved =
      doc && !layer_name.empty() &&
      gis::style::resolve(*doc, nullptr, attrs, zoom_from_scale(map_scale),
                          layer_name, &paint);
  if (resolved) {
    append_resolved_paint_rows(paint, f.kind, out);
  } else if (doc && !layer_name.empty()) {
    out->push_back({"style match", "(no rule for this source-layer / filter)"});
  }

  out->push_back({"--- Feature attributes ---", ""});
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

  out->push_back({"--- GDI / legacy render params (debug) ---", ""});
  out->push_back(
      {"note",
       "SmartGis.exe Reg Color table is legacy-only (edit_config_dock_bar); "
       "Style JSON above is the Views primary path."});
  COLORREF fill = 0;
  COLORREF stroke = 0;
  int stroke_w = 0;
  const Layer* owner = nullptr;
  for (const Layer& layer : layers_) {
    for (const Feature& candidate : layer.features) {
      if (std::memcmp(candidate.id.bytes, f.id.bytes, sizeof(f.id.bytes)) ==
              0 &&
          candidate.id.len == f.id.len) {
        owner = &layer;
        break;
      }
    }
    if (owner) {
      break;
    }
  }
  if (owner &&
      style_colors_for_feature(*owner, f, map_scale, &fill, &stroke,
                               &stroke_w)) {
    out->push_back({"gdi fill (style-driven)", colorref_to_hex(fill)});
    out->push_back({"gdi stroke", colorref_to_hex(stroke)});
    out->push_back({"gdi stroke width", std::to_string(stroke_w)});
  } else {
    out->push_back(
        {"gdi brushes",
         "(Baidu defaults in map2d_gdi_paint when Style JSON unset)"});
  }
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

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/map_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "content/public/feature_attrs.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/ogr_text_encoding.h"
#include "gis/style/style_document.h"
#include "gis/style/style_rules.h"
#include "gis/tile/xyz_math.h"
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

// Cartographic importance 0 (POI) .. 3 (title / province). Drives font size and
// country-scale density gates so prefecture packs stay readable.
int label_importance(const MapScene::Feature& f) {
  const char* cls = field_value(f, "class");
  if (cls) {
    if (std::strcmp(cls, "title") == 0) {
      return 3;
    }
    if (std::strcmp(cls, "region_label") == 0) {
      return 2;
    }
    if (std::strcmp(cls, "river_label") == 0) {
      return 1;
    }
  }
  if (const char* adcode = field_value(f, "adcode")) {
    const size_t n = std::strlen(adcode);
    if (n >= 6) {
      const bool z45 = adcode[4] == '0' && adcode[5] == '0';
      const bool z23 = adcode[2] == '0' && adcode[3] == '0';
      if (z45 && z23) {
        return 3;  // province / municipality
      }
      if (z45) {
        return 2;  // prefecture
      }
      return 1;  // county / district
    }
  }
  const std::string name = feature_display_name(f);
  if (name.empty()) {
    return 0;
  }
  const std::wstring w = gis::datasource::ogr_bytes_to_wide(name);
  auto ends_with = [&w](std::wstring_view suffix) {
    return w.size() >= suffix.size() &&
           w.compare(w.size() - suffix.size(), suffix.size(), suffix) == 0;
  };
  if (ends_with(L"特别行政区") || ends_with(L"自治区") || ends_with(L"省")) {
    return 3;
  }
  if (ends_with(L"自治州") || ends_with(L"地区") || ends_with(L"盟") ||
      ends_with(L"州") || ends_with(L"市")) {
    return 2;
  }
  if (ends_with(L"县") || ends_with(L"区") || ends_with(L"旗") ||
      ends_with(L"镇") || ends_with(L"乡")) {
    return 1;
  }
  if (const char* kind = field_value(f, "kind")) {
    if (std::strcmp(kind, "city") == 0) {
      return 2;
    }
  }
  return 0;
}

int label_font_index(int importance) {
  if (importance >= 3) {
    return 2;
  }
  if (importance >= 2) {
    return 1;
  }
  return 0;
}

struct LabelBudget {
  int min_importance = 0;
  size_t max_labels = 48;
  int cell_w = 80;
  int cell_h = 24;
};

LabelBudget label_budget_for_scale(double scale) {
  // fit_extent on China → scale ~8–15; keep province/prefecture only until zoomed.
  if (scale < 10.0) {
    return {2, 32, 100, 28};
  }
  if (scale < 18.0) {
    return {1, 64, 84, 24};
  }
  return {0, 140, 68, 20};
}

// Screen-space occupancy so overlapping CJK names do not stack into illegible ink.
class LabelOccupancy {
 public:
  LabelOccupancy(int width_px, int height_px, int cell_w, int cell_h)
      : cell_w_(std::max(8, cell_w)),
        cell_h_(std::max(8, cell_h)),
        cols_(std::max(1, (width_px + cell_w_ - 1) / cell_w_)),
        rows_(std::max(1, (height_px + cell_h_ - 1) / cell_h_)) {
    used_.assign(static_cast<size_t>(cols_ * rows_), 0);
  }

  bool try_claim(int x, int y) {
    const int c = x / cell_w_;
    const int r = y / cell_h_;
    if (c < 0 || r < 0 || c >= cols_ || r >= rows_) {
      return false;
    }
    const size_t idx = static_cast<size_t>(r * cols_ + c);
    if (used_[idx]) {
      return false;
    }
    used_[idx] = 1;
    return true;
  }

 private:
  int cell_w_;
  int cell_h_;
  int cols_;
  int rows_;
  std::vector<uint8_t> used_;
};

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

constexpr double kPi = 3.14159265358979323846;

double lon_to_merc_x(double lon) {
  return lon * gis::tile::k_web_mercator_half / 180.0;
}

double lat_to_merc_y(double lat) {
  const double clamped = std::max(-85.05112878, std::min(85.05112878, lat));
  const double rad = clamped * kPi / 180.0;
  return std::log(std::tan(kPi / 4.0 + rad / 2.0)) *
         gis::tile::k_web_mercator_half / kPi;
}

double merc_x_to_lon(double x) {
  return x * 180.0 / gis::tile::k_web_mercator_half;
}

double merc_y_to_lat(double y) {
  const double rad =
      2.0 * (std::atan(std::exp(y * kPi / gis::tile::k_web_mercator_half)) -
             kPi / 4.0);
  return rad * 180.0 / kPi;
}

COLORREF argb_to_colorref(uint32_t argb) {
  return RGB(static_cast<int>((argb >> 16) & 0xFF),
             static_cast<int>((argb >> 8) & 0xFF),
             static_cast<int>(argb & 0xFF));
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

bool write_bmp_file(const std::string& path, int width_px, int height_px,
                    const void* bits, int stride_bytes) {
  if (path.empty() || !bits || width_px <= 0 || height_px <= 0 ||
      stride_bytes <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride_bytes) * static_cast<DWORD>(height_px);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;  // 'BM'
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width_px;
  bih.biHeight = height_px;  // bottom-up
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
    return false;
  }
  const bool ok =
      std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
      std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
      std::fwrite(bits, 1, image_bytes, f) == image_bytes;
  std::fclose(f);
  return ok;
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
  basemap_tiles_drawn_ = 0;
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
  basemap_tiles_drawn_ = 0;
}

void MapScene::clear_basemap_provider() {
  basemap_.reset();
  basemap_tiles_drawn_ = 0;
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
                                       COLORREF* fill, COLORREF* stroke,
                                       int* stroke_width) const {
  if (!style_doc_ || !fill || !stroke || !stroke_width) {
    return false;
  }
  gis::style::AttrMap attrs;
  for (const Field& field : f.fields) {
    attrs[field.name] = field.value;
  }
  gis::style::ResolvedPaint paint;
  if (!gis::style::resolve(*style_doc_, nullptr, attrs, zoom_from_scale(scale_),
                           layer.name, &paint)) {
    return false;
  }
  switch (f.kind) {
    case GeomKind::kPolygon:
      *fill = argb_to_colorref(paint.fill_color);
      *stroke = map_scene_admin_stroke_color();
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

void MapScene::paint_basemap_underlay(HDC hdc, int width_px,
                                     int height_px) const {
  basemap_tiles_drawn_ = 0;
  if (!hdc || !basemap_ || !basemap_->is_open() || width_px <= 0 ||
      height_px <= 0) {
    return;
  }
  const content::Extent2 world = view_world_extent(width_px, height_px);
  gis::tile::Viewport vp;
  vp.min_x = lon_to_merc_x(world.xmin);
  vp.max_x = lon_to_merc_x(world.xmax);
  vp.min_y = lat_to_merc_y(world.ymin);
  vp.max_y = lat_to_merc_y(world.ymax);
  if (vp.min_x > vp.max_x) {
    std::swap(vp.min_x, vp.max_x);
  }
  if (vp.min_y > vp.max_y) {
    std::swap(vp.min_y, vp.max_y);
  }
  const std::vector<gis::tile::TileImage> tiles =
      basemap_->fetch_visible(vp, 2);
  basemap_tiles_drawn_ = tiles.size();
  for (const gis::tile::TileImage& tile : tiles) {
    const double lon0 = merc_x_to_lon(tile.world_rect.lb.x);
    const double lon1 = merc_x_to_lon(tile.world_rect.rt.x);
    const double lat0 = merc_y_to_lat(tile.world_rect.lb.y);
    const double lat1 = merc_y_to_lat(tile.world_rect.rt.y);
    // Map-space Y is -lat.
    int vx0 = 0;
    int vy0 = 0;
    int vx1 = 0;
    int vy1 = 0;
    map_to_view(lon0, -lat1, &vx0, &vy0);
    map_to_view(lon1, -lat0, &vx1, &vy1);
    if (vx0 > vx1) {
      std::swap(vx0, vx1);
    }
    if (vy0 > vy1) {
      std::swap(vy0, vy1);
    }
    if (vx1 < 0 || vy1 < 0 || vx0 > width_px || vy0 > height_px) {
      continue;
    }
    // Encoded PNG decode is optional; a tinted quad proves underlay coverage.
    const int shade = 90 + ((tile.coord.x + tile.coord.y) & 7) * 12;
    HBRUSH brush =
        CreateSolidBrush(RGB(shade, 110 + (tile.coord.z % 5) * 8, 140));
    RECT rc = {vx0, vy0, vx1 + 1, vy1 + 1};
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
  }
}

bool MapScene::export_bmp(const std::string& path, int width_px,
                         int height_px) const {
  if (path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  if (!mem) {
    ReleaseDC(nullptr, screen);
    return false;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = height_px;  // bottom-up for BMP write
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  paint(mem, width_px, height_px, true);
  const int stride = ((width_px * 32 + 31) / 32) * 4;
  const bool ok = write_bmp_file(path, width_px, height_px, bits, stride);
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return ok;
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

  paint_basemap_underlay(hdc, width_px, height_px);

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
  // Style-driven colors: cache pens/brushes per COLORREF for this frame only.
  std::map<COLORREF, HBRUSH> style_brushes;
  std::map<uint64_t, HPEN> style_pens;
  auto style_brush = [&](COLORREF c) -> HBRUSH {
    auto it = style_brushes.find(c);
    if (it != style_brushes.end()) {
      return it->second;
    }
    HBRUSH b = CreateSolidBrush(c);
    style_brushes.emplace(c, b);
    return b;
  };
  auto style_pen = [&](COLORREF c, int width) -> HPEN {
    const uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 16) |
        static_cast<uint64_t>(static_cast<uint16_t>(std::max(1, width)));
    auto it = style_pens.find(key);
    if (it != style_pens.end()) {
      return it->second;
    }
    HPEN p = CreatePen(PS_SOLID, std::max(1, width), c);
    style_pens.emplace(key, p);
    return p;
  };
  HFONT fonts[3] = {};
  // Baidu-like hierarchy: small body / medium prefecture / large province title.
  fonts[0] = CreateFontW(dip_px(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  fonts[1] = CreateFontW(dip_px(16), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  fonts[2] = CreateFontW(dip_px(22), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  HFONT status_font =
      CreateFontW(dip_px(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
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
  const LabelBudget label_budget = label_budget_for_scale(scale_);

  struct PendingLabel {
    int vx = 0;
    int vy = 0;
    int dx = 0;
    int dy = 0;
    int importance = 0;
    COLORREF ink = RGB(20, 18, 14);
    std::string name;
  };
  std::vector<PendingLabel> pending_labels;
  pending_labels.reserve(label_budget.max_labels * 2);
  auto queue_place_label = [&](int vx, int vy, const Feature& f, COLORREF ink,
                               int dx, int dy) {
    const int importance = label_importance(f);
    if (importance < label_budget.min_importance) {
      return;
    }
    std::string name = feature_display_name(f);
    if (name.empty()) {
      return;
    }
    if (vx + dx < -80 || vy + dy < -40 || vx + dx > width_px + 80 ||
        vy + dy > height_px + 40) {
      return;
    }
    pending_labels.push_back(
        {vx, vy, dx, dy, importance, ink, std::move(name)});
  };

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
          COLORREF ink = RGB(20, 18, 14);
          if (cls && std::strcmp(cls, "title") == 0) {
            ink = RGB(12, 10, 8);
          } else if (cls && std::strcmp(cls, "river_label") == 0) {
            ink = RGB(8, 36, 72);
          }
          if (f.selected) {
            ink = RGB(200, 120, 40);
          }
          queue_place_label(vx, vy, f, ink, -20, -8);
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
          COLORREF fill_c = map_scene_point_fill_color();
          COLORREF stroke_c = fill_c;
          int stroke_w = 1;
          const bool styled =
              !f.selected &&
              style_colors_for_feature(layer, f, &fill_c, &stroke_c, &stroke_w);
          HBRUSH fill_brush =
              f.selected ? selected_brush
                         : (styled ? style_brush(fill_c) : point_brush);
          SelectObject(hdc, fill_brush);
          SelectObject(hdc, f.selected ? pens[3] : pens[0]);
          Ellipse(hdc, vx - r, vy - r, vx + r + 1, vy + r + 1);
          SelectObject(hdc, GetStockObject(NULL_BRUSH));
          if (draw_dense_text && !has_text_features) {
            queue_place_label(vx, vy, f, RGB(40, 32, 20), 7, -8);
          }
          continue;
        }

        if (f.kind == GeomKind::kLine) {
          const char* cls = field_value(f, "class");
          const char* kind = field_value(f, "kind");
          HPEN pen = pens[2];
          COLORREF fill_c = 0;
          COLORREF stroke_c = 0;
          int stroke_w = 2;
          if (f.selected) {
            pen = pens[3];
          } else if (style_colors_for_feature(layer, f, &fill_c, &stroke_c,
                                              &stroke_w)) {
            pen = style_pen(stroke_c, stroke_w);
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
          COLORREF fill_c = map_scene_area_fill_color(nullptr, 0);
          COLORREF stroke_c = map_scene_admin_stroke_color();
          int stroke_w = 1;
          const bool styled =
              !f.selected &&
              style_colors_for_feature(layer, f, &fill_c, &stroke_c, &stroke_w);
          SelectObject(hdc, f.selected
                                ? pens[3]
                                : (styled ? style_pen(stroke_c, stroke_w)
                                          : pens[0]));
          HBRUSH fill = f.selected
                            ? selected_brush
                            : (styled ? style_brush(fill_c) : land_brush);
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
            queue_place_label(lvx, lvy, f, RGB(32, 28, 22), -12, -6);
          }
        }
      }
    }
  }

  // Higher-importance labels claim occupancy first (province before county).
  std::sort(pending_labels.begin(), pending_labels.end(),
            [](const PendingLabel& a, const PendingLabel& b) {
              if (a.importance != b.importance) {
                return a.importance > b.importance;
              }
              return a.name.size() < b.name.size();
            });
  LabelOccupancy label_occ(width_px, height_px, label_budget.cell_w,
                           label_budget.cell_h);
  size_t labels_drawn = 0;
  for (const PendingLabel& lab : pending_labels) {
    if (labels_drawn >= label_budget.max_labels) {
      break;
    }
    const int ax = lab.vx + lab.dx;
    const int ay = lab.vy + lab.dy;
    if (!label_occ.try_claim(ax, ay)) {
      continue;
    }
    draw_label(lab.vx, lab.vy, lab.name, lab.ink,
               label_font_index(lab.importance), lab.dx, lab.dy);
    ++labels_drawn;
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
  for (auto& kv : style_brushes) {
    if (kv.second) {
      DeleteObject(kv.second);
    }
  }
  for (auto& kv : style_pens) {
    if (kv.second) {
      DeleteObject(kv.second);
    }
  }

  // Opaque status strip — avoids ghosting / illegible overlap on dense labels.
  SetBkMode(hdc, TRANSPARENT);
  SelectObject(hdc, status_font ? status_font : stock_font);
  wchar_t line[160];
  swprintf_s(line, L"Layers %zu  Features %zu  scale %.4g%s", layers_.size(),
             feature_count(), scale_, last_open_was_ogr_ ? L"  OGR" : L"");
  const int status_y = height_px > 48 ? height_px - 36 : 12;
  SIZE text_sz = {};
  GetTextExtentPoint32W(hdc, line, lstrlenW(line), &text_sz);
  RECT status_rc = {8, status_y - 4, 16 + text_sz.cx, status_y + text_sz.cy + 4};
  HBRUSH status_bg = CreateSolidBrush(RGB(245, 243, 233));
  FillRect(hdc, &status_rc, status_bg);
  DeleteObject(status_bg);
  SetTextColor(hdc, RGB(24, 28, 32));
  TextOutW(hdc, 12, status_y, line, lstrlenW(line));
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

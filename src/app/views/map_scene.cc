// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/map_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

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
  for (int i = 0; i < n; ++i) {
    // Flip Y so screen +Y is down while GIS +Y stays north-up after fit.
    out->push_back({ring->getX(i), -ring->getY(i)});
  }
}

bool feature_from_ogr(OGRFeature* ogr_feat, MapScene::Feature* out,
                      content::FeatureId id) {
  if (!ogr_feat || !out) {
    return false;
  }
  OGRGeometry* geom = ogr_feat->GetGeometryRef();
  if (!geom || geom->IsEmpty()) {
    return false;
  }
  out->id = id;
  out->points.clear();
  out->fields.clear();
  out->selected = false;

  OGRFeatureDefn* defn = ogr_feat->GetDefnRef();
  if (defn) {
    const int field_count = defn->GetFieldCount();
    for (int i = 0; i < field_count && static_cast<int>(out->fields.size()) < 8;
         ++i) {
      if (!ogr_feat->IsFieldSetAndNotNull(i)) {
        continue;
      }
      OGRFieldDefn* fd = defn->GetFieldDefn(i);
      if (!fd) {
        continue;
      }
      out->fields.push_back({fd->GetNameRef() ? fd->GetNameRef() : "field",
                             ogr_feat->GetFieldAsString(i)});
    }
  }

  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPoint) {
    auto* pt = geom->toPoint();
    out->kind = MapScene::GeomKind::kPoint;
    out->points.push_back({pt->getX(), -pt->getY()});
    return true;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    out->kind = MapScene::GeomKind::kLine;
    append_ring(geom->toLineString(), &out->points);
    return out->points.size() >= 2;
  }
  if (flat == wkbPolygon) {
    out->kind = MapScene::GeomKind::kPolygon;
    OGRPolygon* poly = geom->toPolygon();
    if (OGRLinearRing* ext = poly->getExteriorRing()) {
      append_ring(ext, &out->points);
    }
    return out->points.size() >= 3;
  }
  if (flat == wkbMultiPoint) {
    auto* multi = geom->toMultiPoint();
    if (multi->getNumGeometries() < 1) {
      return false;
    }
    auto* pt = multi->getGeometryRef(0)->toPoint();
    out->kind = MapScene::GeomKind::kPoint;
    out->points.push_back({pt->getX(), -pt->getY()});
    return true;
  }
  if (flat == wkbMultiLineString) {
    auto* multi = geom->toMultiLineString();
    if (multi->getNumGeometries() < 1) {
      return false;
    }
    out->kind = MapScene::GeomKind::kLine;
    append_ring(multi->getGeometryRef(0)->toLineString(), &out->points);
    return out->points.size() >= 2;
  }
  if (flat == wkbMultiPolygon) {
    auto* multi = geom->toMultiPolygon();
    if (multi->getNumGeometries() < 1) {
      return false;
    }
    out->kind = MapScene::GeomKind::kPolygon;
    OGRPolygon* poly = multi->getGeometryRef(0)->toPolygon();
    if (OGRLinearRing* ext = poly->getExteriorRing()) {
      append_ring(ext, &out->points);
    }
    return out->points.size() >= 3;
  }
  return false;
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

void MapScene::seed_default() {
  clear();
  Layer layer;
  layer.id = "layer.demo";
  layer.name = "Demo layer";
  layer.visible = true;
  add_sample_features(&layer, "demo");
  active_layer_id_ = layer.id;
  layers_.push_back(std::move(layer));
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
      Feature f;
      if (!feature_from_ogr(feat.get(), &f, next_feature_id())) {
        continue;
      }
      layer.features.push_back(std::move(f));
      ++taken;
      ++total_features;
    }
    if (!layer.features.empty()) {
      loaded.push_back(std::move(layer));
    }
  }
  if (loaded.empty() || total_features == 0) {
    return false;
  }
  layers_ = std::move(loaded);
  active_layer_id_ = layers_.front().id;
  selected_id_ = {};
  return true;
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

std::vector<ui::views::LayerTree::LayerDesc> MapScene::layer_descs() const {
  std::vector<ui::views::LayerTree::LayerDesc> out;
  out.reserve(layers_.size());
  for (const Layer& layer : layers_) {
    ui::views::LayerTree::LayerDesc d;
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
      if (fit->kind == GeomKind::kPoint) {
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
  if (factor <= 0.0) {
    return;
  }
  double mx = 0;
  double my = 0;
  view_to_map(view_x, view_y, &mx, &my);
  scale_ *= factor;
  if (scale_ < 0.0001) {
    scale_ = 0.0001;
  }
  if (scale_ > 1.0e6) {
    scale_ = 1.0e6;
  }
  pan_x_ = static_cast<double>(view_x) - mx * scale_;
  pan_y_ = static_cast<double>(view_y) - my * scale_;
}

void MapScene::fit_extent(int view_w, int view_h) {
  if (view_w <= 0) {
    view_w = 800;
  }
  if (view_h <= 0) {
    view_h = 600;
  }
  bool have = false;
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  for (const Layer& layer : layers_) {
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
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }
  for (const Layer& layer : layers_) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (f.points.empty()) {
        continue;
      }
      const COLORREF color =
          f.selected ? RGB(255, 220, 60) : RGB(90, 200, 255);
      const int pen_w = f.selected ? 3 : 2;
      HPEN pen = CreatePen(PS_SOLID, pen_w, color);
      HGDIOBJ old_pen = SelectObject(hdc, pen);
      HBRUSH brush = CreateSolidBrush(color);
      HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

      if (f.kind == GeomKind::kPoint) {
        int vx = 0;
        int vy = 0;
        map_to_view(f.points[0].x, f.points[0].y, &vx, &vy);
        SelectObject(hdc, brush);
        Ellipse(hdc, vx - 5, vy - 5, vx + 5, vy + 5);
      } else if (f.kind == GeomKind::kLine) {
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
      } else {
        std::vector<POINT> pts;
        pts.reserve(f.points.size());
        for (const Vertex& p : f.points) {
          int vx = 0;
          int vy = 0;
          map_to_view(p.x, p.y, &vx, &vy);
          pts.push_back({vx, vy});
        }
        if (pts.size() >= 3) {
          SelectObject(hdc, brush);
          Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
        }
      }

      SelectObject(hdc, old_brush);
      SelectObject(hdc, old_pen);
      DeleteObject(brush);
      DeleteObject(pen);
    }
  }

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(230, 240, 250));
  wchar_t line[128];
  swprintf_s(line, L"Layers %zu  Features %zu  scale %.4g%s", layers_.size(),
             feature_count(), scale_, last_open_was_ogr_ ? L"  OGR" : L"");
  TextOutW(hdc, 12, height_px > 48 ? height_px - 36 : 12, line, lstrlenW(line));
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
                              : (f.kind == GeomKind::kPolygon ? "polygon"
                                                              : "point")});
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
      std::string name = f.fields.empty() ? feature_token(f.id) : f.fields[0].value;
      std::string type = "point";
      for (const Field& field : f.fields) {
        if (field.name == "type") {
          type = field.value;
        }
        if (field.name == "name") {
          name = field.value;
        }
      }
      rows->push_back({feature_token(f.id), name, type, layer.name});
      tokens->push_back(feature_token(f.id));
    }
  }
}

std::string MapScene::feature_token(const content::FeatureId& id) {
  std::string token = "fid:";
  for (uint8_t i = 0; i < id.len && i < sizeof(id.bytes); ++i) {
    char hex[3];
    std::snprintf(hex, sizeof(hex), "%02x",
                  static_cast<unsigned>(id.bytes[i]));
    token += hex;
  }
  return token;
}

content::FeatureId MapScene::feature_id_from_token(const std::string& token) {
  content::FeatureId id{};
  std::string_view hex = token;
  constexpr std::string_view kPrefix = "fid:";
  if (hex.size() >= kPrefix.size() && hex.substr(0, kPrefix.size()) == kPrefix) {
    hex.remove_prefix(kPrefix.size());
  }
  size_t n = 0;
  for (size_t i = 0; i + 1 < hex.size() && n < sizeof(id.bytes); i += 2) {
    unsigned v = 0;
    if (std::sscanf(hex.data() + i, "%2x", &v) != 1) {
      break;
    }
    id.bytes[n++] = static_cast<uint8_t>(v);
  }
  id.len = static_cast<uint8_t>(n);
  return id;
}

bool MapScene::update_feature_field(const std::string& token,
                                    const std::string& field,
                                    const std::string& value) {
  const content::FeatureId id = feature_id_from_token(token);
  Feature* f = find_feature(id);
  if (!f || field.empty()) {
    return false;
  }
  for (Field& item : f->fields) {
    if (item.name == field) {
      item.value = value;
      return true;
    }
  }
  f->fields.push_back({field, value});
  return true;
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
  if (vx) {
    *vx = static_cast<int>(std::lround(mx * scale_ + pan_x_));
  }
  if (vy) {
    *vy = static_cast<int>(std::lround(my * scale_ + pan_y_));
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

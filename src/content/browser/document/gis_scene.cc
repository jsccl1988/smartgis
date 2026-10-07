// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/gis_scene.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/document/edit/feature_edit.h"
#include "content/browser/document/ingest/geojson_write.h"
#include "content/browser/document/ingest/ogr_ingest.h"
#include "content/browser/document/ingest/seed_paths.h"
#include "content/browser/document/query/extent_query.h"
#include "content/browser/document/query/inspector.h"
#include "content/public/types.h"
#include "base/trace/event/process_trace.h"
#include "gis/style/document/style_document.h"

namespace content {

GisScene::GisScene() = default;

GisScene::~GisScene() = default;

void GisScene::clear() {
  store_.clear();
}

void GisScene::set_style_document(
    std::shared_ptr<gis::style::StyleDocument> doc) {
  style_.set_style_document(std::move(doc));
}

void GisScene::clear_style_document() {
  style_.clear_style_document();
}

bool GisScene::load_style_path(const std::string& path) {
  return style_.load_style_path(path);
}

void GisScene::set_basemap_provider(
    std::shared_ptr<gis::tile::TileProvider> provider) {
  style_.set_basemap_provider(std::move(provider));
}

void GisScene::clear_basemap_provider() {
  style_.clear_basemap_provider();
}

bool GisScene::resolve_style_for_test(const std::string& source_layer,
                                     const gis::style::AttrMap& attrs,
                                     double zoom,
                                     gis::style::ResolvedPaint* out) const {
  return style_.resolve_style_for_test(source_layer, attrs, zoom, out);
}

bool GisScene::style_colors_for_feature(const detail::GisLayer& layer,
                                       const detail::GisFeature& f,
                                       double scale, COLORREF* fill,
                                       COLORREF* stroke,
                                       int* stroke_width) const {
  return style_.style_colors_for_feature(layer, f, scale, fill, stroke,
                                         stroke_width);
}

namespace {

std::string module_exe_dir() {
  char exe_path[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    if (exe_path[i] == '\\' || exe_path[i] == '/') {
      exe_path[i + 1] = '\0';
      break;
    }
  }
  return exe_path;
}

}  // namespace

bool GisScene::try_bootstrap_china_plp() {
  const std::string exe_dir = module_exe_dir();
  if (exe_dir.empty()) {
    return false;
  }
  std::string path;
  if (!try_resolve_china_seed_path(exe_dir, &path)) {
    return false;
  }
  // Default china seed uses null StyleDocument + default_carto_style_json; skip
  // accompanying *.style.json I/O (china_city.style.json is refused anyway).
  return open_path(path, /*load_accompanying_style=*/false);
}

void GisScene::seed_default(bool allow_china_bootstrap) {
  if (allow_china_bootstrap) {
    BASE_TRACE_EVENT("SeedDocument.ChinaBootstrap", "startup");
    if (try_bootstrap_china_plp()) {
      // Match --map2d-showcase=china: clear any accompanying china_city.style.json
      // so Map2dFrameCache uses default MapLibre carto + carto_source_layer remap
      // (area→land, lines→river/admin/road). Binding the file style keys
      // source-layer area/line/point and disables remap — cream wash + blue
      // scribble / black point squares that diverge from the shot gates.
      clear_style_document();
      return;
    }
  }
  // Real-data policy: never invent demo features when china packs are missing.
  clear();
}

bool GisScene::compute_extent(double* min_x, double* min_y, double* max_x,
                             double* max_y) const {
  return detail::compute_extent(store_, min_x, min_y, max_x, max_y);
}

bool GisScene::has_china_extent() const {
  return detail::has_china_extent(store_);
}

bool GisScene::open_path(const std::string& path) {
  return open_path(path, true);
}

bool GisScene::open_path(const std::string& path,
                         bool load_accompanying_style) {
  const std::string stem = detail::path_stem(path);
  if (stem.empty()) {
    return false;
  }
  if (detail::ingest_ogr_path(&store_, path)) {
    store_.set_last_open_was_ogr(true);
    if (load_accompanying_style) {
      detail::try_load_accompanying_style(
          [this](const std::string& style_path) {
            return load_style_path(style_path);
          },
          path);
    }
    return true;
  }
  // Real-data policy: do not invent sample features on OGR miss.
  store_.set_last_open_was_ogr(false);
  (void)stem;
  return false;
}

bool GisScene::write_path(const std::string& path) const {
  return detail::write_geojson_path(store_, path);
}

std::vector<GisScene::LayerDesc> GisScene::layer_descs() const {
  std::vector<LayerDesc> out = store_.layer_descs();
  // Tile basemap lives on StyleBind, not GisLayer. When the catalog command
  // path created a kRaster layer, skip; otherwise expose a synthetic leaf so
  // TOC can show kind without inventing extra business overlays.
  if (!has_basemap_provider()) {
    return out;
  }
  const bool has_raster_leaf = [&out] {
    std::function<bool(const LayerDesc&)> walk = [&](const LayerDesc& d) {
      if (d.kind == LayerKind::kRaster) {
        return true;
      }
      for (const LayerDesc& child : d.children) {
        if (walk(child)) {
          return true;
        }
      }
      return false;
    };
    for (const LayerDesc& d : out) {
      if (walk(d)) {
        return true;
      }
    }
    return false;
  }();
  if (has_raster_leaf) {
    return out;
  }
  LayerDesc basemap;
  basemap.id = "basemap.tiles";
  basemap.name = "Basemap";
  basemap.visible = true;
  basemap.active = false;
  basemap.kind = LayerKind::kRaster;
  basemap.expanded = true;
  out.insert(out.begin(), std::move(basemap));
  return out;
}

size_t GisScene::feature_count() const {
  return store_.feature_count();
}

bool GisScene::create_layer(const std::string& name,
                            const std::string& geometry_type) {
  content::LayerKind kind = content::LayerKind::kVector;
  if (geometry_type == "xyz" || geometry_type == "wmts" ||
      geometry_type == "raster") {
    kind = content::LayerKind::kRaster;
  }
  return store_.create_layer(name, kind);
}

bool GisScene::remove_layer(const std::string& id) {
  return store_.remove_layer(id);
}

bool GisScene::set_layer_visible(const std::string& id, bool visible) {
  return store_.set_layer_visible(id, visible);
}

bool GisScene::select_layer(const std::string& id) {
  return store_.select_layer(id);
}

bool GisScene::move_layer(const std::string& id, int delta) {
  return store_.move_layer(id, delta);
}

content::FeatureId GisScene::append_from_draft(
    const tool::Draft& draft, const char* tool_id,
    const std::function<void(int view_x, int view_y, double* map_x,
                             double* map_y)>& to_map) {
  ensure_active_layer();
  return detail::append_from_draft(&store_, draft, tool_id, to_map);
}

content::FeatureId GisScene::move_selected_vertex(double map_x, double map_y,
                                                 double tol_map) {
  return detail::move_selected_vertex(&store_, map_x, map_y, tol_map);
}

bool GisScene::copy_feature_xy(
    const content::FeatureId& id,
    std::vector<std::pair<double, double>>* out) const {
  return detail::copy_feature_xy(store_, id, out);
}

bool GisScene::add_triangle_layer(const std::string& name, const double* xyz,
                                 int point_count, const int* triangles,
                                 int triangle_count) {
  return detail::add_triangle_layer(&store_, name, xyz, point_count, triangles,
                                    triangle_count);
}

bool GisScene::add_point_cloud_layer(const std::string& name, const float* xyz,
                                     int point_count, const uint8_t* rgba) {
  return detail::add_point_cloud_layer(&store_, name, xyz, point_count, rgba);
}

const GisScene::Feature* GisScene::hit_test(double map_x, double map_y,
                                           double tol_map) {
  return detail::hit_test(&store_, map_x, map_y, tol_map);
}

std::vector<const GisScene::Feature*> GisScene::hit_test_all(double map_x,
                                                            double map_y,
                                                            double tol_map) {
  return detail::hit_test_all(&store_, map_x, map_y, tol_map);
}

bool GisScene::select_feature(const content::FeatureId& id) {
  return store_.select_feature(id);
}

void GisScene::clear_selection() {
  store_.clear_selection();
}

const GisScene::Feature* GisScene::selected_feature() const {
  return store_.selected_feature();
}

bool GisScene::active_layer_world_extent(content::Extent2* out) const {
  return detail::active_layer_world_extent(store_, out);
}

bool GisScene::selection_world_extent(content::Extent2* out) const {
  return detail::selection_world_extent(store_, out);
}

content::Extent2 GisScene::world_extent() const {
  return detail::world_extent(store_);
}

void GisScene::export_land_rings(std::vector<vista::LonLatRing>* out) const {
  detail::export_land_rings(store_, out);
}

bool GisScene::polygon_fit_box(double* min_x, double* min_y, double* max_x,
                               double* max_y) const {
  return detail::polygon_fit_box(store_, min_x, min_y, max_x, max_y);
}

void GisScene::fill_feature_info_fields(
    const Feature& f, std::vector<std::pair<std::string, std::string>>* out,
    const std::string& source_layer, double map_scale) const {
  detail::fill_feature_info_fields(store_, style_, f, out, source_layer,
                                   map_scale);
}

void GisScene::fill_attribute_rows(std::vector<std::string>* columns,
                                   std::vector<std::vector<std::string>>* rows,
                                   std::vector<std::string>* tokens) const {
  detail::fill_attribute_rows(store_, columns, rows, tokens);
}

std::string GisScene::feature_token(const content::FeatureId& id) {
  return content::encode_feature_token(id);
}

content::FeatureId GisScene::feature_id_from_token(const std::string& token) {
  return content::decode_feature_token(token);
}

bool GisScene::update_feature_field(const std::string& token,
                                    const std::string& field,
                                    const std::string& value) {
  Feature* f = store_.find_feature(feature_id_from_token(token));
  if (!f) {
    return false;
  }
  return content::apply_named_field(&f->fields, field, value);
}

void GisScene::ensure_active_layer() {
  if (store_.find_layer(store_.active_layer_id())) {
    return;
  }
  if (store_.layers().empty()) {
    seed_default();
    return;
  }
  store_.set_active_layer_id(store_.layers().front().id);
}

}  // namespace content

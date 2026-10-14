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
#include "content/browser/document/store/layer_store.h"
#include "content/browser/document/style/style_bind.h"
#include "content/public/types.h"
#include "base/trace/event/process_trace.h"
#include "gis/style/document/style_document.h"
#include "gis/tile/provider/tile_provider.h"
#include "vista/terrain/dem/mask/land_mask.h"
#include "vista/terrain/dem/raster/dem_raster.h"

namespace content {

struct GisScene::Impl {
  detail::LayerStore store;
  detail::StyleBind style;
};

GisScene::GisScene() : impl_(std::make_unique<Impl>()) {}

GisScene::~GisScene() = default;

void GisScene::clear() {
  impl_->store.clear();
}

void GisScene::set_style_document(
    std::shared_ptr<gis::style::StyleDocument> doc) {
  impl_->style.set_style_document(std::move(doc));
}

void GisScene::clear_style_document() {
  impl_->style.clear_style_document();
}

bool GisScene::load_style_path(const std::string& path) {
  return impl_->style.load_style_path(path);
}

bool GisScene::has_style_document() const {
  return impl_->style.has_style_document();
}

const gis::style::StyleDocument* GisScene::style_document() const {
  return impl_->style.style_document();
}

std::shared_ptr<gis::style::StyleDocument> GisScene::style_document_shared()
    const {
  return impl_->style.style_document_shared();
}

void GisScene::set_basemap_provider(
    std::shared_ptr<gis::tile::TileProvider> provider) {
  impl_->style.set_basemap_provider(std::move(provider));
}

void GisScene::clear_basemap_provider() {
  impl_->style.clear_basemap_provider();
}

bool GisScene::has_basemap_provider() const {
  return impl_->style.has_basemap_provider();
}

bool GisScene::resolve_style_for_test(const std::string& source_layer,
                                      const gis::style::AttrMap& attrs,
                                      double zoom,
                                      gis::style::ResolvedPaint* out) const {
  return impl_->style.resolve_style_for_test(source_layer, attrs, zoom, out);
}

bool GisScene::style_colors_for_feature(const Layer& layer, const Feature& f,
                                        double scale, COLORREF* fill,
                                        COLORREF* stroke,
                                        int* stroke_width) const {
  return impl_->style.style_colors_for_feature(layer, f, scale, fill, stroke,
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

bool layer_descs_have_raster(const std::vector<GisScene::LayerDesc>& out) {
  std::function<bool(const GisScene::LayerDesc&)> walk =
      [&](const GisScene::LayerDesc& d) {
        if (d.kind == LayerKind::kRaster) {
          return true;
        }
        for (const GisScene::LayerDesc& child : d.children) {
          if (walk(child)) {
            return true;
          }
        }
        return false;
      };
  for (const GisScene::LayerDesc& d : out) {
    if (walk(d)) {
      return true;
    }
  }
  return false;
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
  return detail::compute_extent(impl_->store, min_x, min_y, max_x, max_y);
}

bool GisScene::has_drawable_extent() const {
  return detail::has_drawable_extent(impl_->store);
}

bool GisScene::has_land_surface() const {
  return detail::has_land_surface(impl_->store);
}

bool GisScene::land_surface_visible() const {
  return detail::land_surface_visible(impl_->store);
}

bool GisScene::has_resolvable_dem() const {
  return !vista::find_sample_dem_path().empty();
}

bool GisScene::can_attach_hillshade() const {
  return has_drawable_extent() && land_surface_visible() &&
         has_resolvable_dem();
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
  if (detail::ingest_ogr_path(&impl_->store, path)) {
    impl_->store.set_last_open_was_ogr(true);
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
  impl_->store.set_last_open_was_ogr(false);
  (void)stem;
  return false;
}

bool GisScene::write_path(const std::string& path) const {
  return detail::write_geojson_path(impl_->store, path);
}

std::vector<GisScene::LayerDesc> GisScene::layer_descs() const {
  std::vector<LayerDesc> out = impl_->store.layer_descs();
  // Tile basemap lives on StyleBind, not MapLayer. When the catalog command
  // path created a kRaster layer, skip; otherwise expose a synthetic leaf so
  // TOC can show kind without inventing extra business overlays.
  if (!has_basemap_provider() || layer_descs_have_raster(out)) {
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

const std::string& GisScene::active_layer_id() const {
  return impl_->store.active_layer_id();
}

size_t GisScene::layer_count() const {
  return impl_->store.layers().size();
}

size_t GisScene::feature_count() const {
  return impl_->store.feature_count();
}

bool GisScene::last_open_was_ogr() const {
  return impl_->store.last_open_was_ogr();
}

const std::vector<GisScene::Layer>& GisScene::layers() const {
  return impl_->store.layers();
}

gis::tile::TileProvider* GisScene::basemap_provider() const {
  return impl_->style.basemap_provider();
}

bool GisScene::create_layer(const std::string& name,
                            const std::string& geometry_type) {
  content::LayerKind kind = content::LayerKind::kVector;
  if (geometry_type == "xyz" || geometry_type == "wmts" ||
      geometry_type == "raster") {
    kind = content::LayerKind::kRaster;
  }
  return impl_->store.create_layer(name, kind);
}

bool GisScene::remove_layer(const std::string& id) {
  return impl_->store.remove_layer(id);
}

bool GisScene::set_layer_visible(const std::string& id, bool visible) {
  return impl_->store.set_layer_visible(id, visible);
}

bool GisScene::select_layer(const std::string& id) {
  return impl_->store.select_layer(id);
}

bool GisScene::move_layer(const std::string& id, int delta) {
  return impl_->store.move_layer(id, delta);
}

content::FeatureId GisScene::append_from_draft(
    const tool::Draft& draft, const char* tool_id,
    const std::function<void(int view_x, int view_y, double* map_x,
                             double* map_y)>& to_map) {
  ensure_active_layer();
  return detail::append_from_draft(&impl_->store, draft, tool_id, to_map);
}

content::FeatureId GisScene::move_selected_vertex(double map_x, double map_y,
                                                  double tol_map) {
  return detail::move_selected_vertex(&impl_->store, map_x, map_y, tol_map);
}

bool GisScene::copy_feature_xy(
    const content::FeatureId& id,
    std::vector<std::pair<double, double>>* out) const {
  return detail::copy_feature_xy(impl_->store, id, out);
}

bool GisScene::add_triangle_layer(const std::string& name, const double* xyz,
                                  int point_count, const int* triangles,
                                  int triangle_count) {
  return detail::add_triangle_layer(&impl_->store, name, xyz, point_count,
                                    triangles, triangle_count);
}

bool GisScene::add_point_cloud_layer(const std::string& name, const float* xyz,
                                     int point_count, const uint8_t* rgba) {
  return detail::add_point_cloud_layer(&impl_->store, name, xyz, point_count,
                                       rgba);
}

const GisScene::Feature* GisScene::hit_test(double map_x, double map_y,
                                            double tol_map) {
  return detail::hit_test(&impl_->store, map_x, map_y, tol_map);
}

std::vector<const GisScene::Feature*> GisScene::hit_test_all(double map_x,
                                                             double map_y,
                                                             double tol_map) {
  return detail::hit_test_all(&impl_->store, map_x, map_y, tol_map);
}

bool GisScene::select_feature(const content::FeatureId& id) {
  return impl_->store.select_feature(id);
}

void GisScene::clear_selection() {
  impl_->store.clear_selection();
}

const GisScene::Feature* GisScene::selected_feature() const {
  return impl_->store.selected_feature();
}

bool GisScene::active_layer_world_extent(content::Extent2* out) const {
  return detail::active_layer_world_extent(impl_->store, out);
}

bool GisScene::selection_world_extent(content::Extent2* out) const {
  return detail::selection_world_extent(impl_->store, out);
}

content::Extent2 GisScene::world_extent() const {
  return detail::world_extent(impl_->store);
}

void GisScene::export_land_rings(std::vector<vista::LonLatRing>* out) const {
  detail::export_land_rings(impl_->store, out);
}

bool GisScene::polygon_fit_box(double* min_x, double* min_y, double* max_x,
                               double* max_y) const {
  return detail::polygon_fit_box(impl_->store, min_x, min_y, max_x, max_y);
}

void GisScene::fill_feature_info_fields(
    const Feature& f, std::vector<std::pair<std::string, std::string>>* out,
    const std::string& source_layer, double map_scale) const {
  detail::fill_feature_info_fields(impl_->store, impl_->style, f, out,
                                   source_layer, map_scale);
}

void GisScene::fill_attribute_rows(std::vector<std::string>* columns,
                                   std::vector<std::vector<std::string>>* rows,
                                   std::vector<std::string>* tokens) const {
  detail::fill_attribute_rows(impl_->store, columns, rows, tokens);
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
  Feature* f = impl_->store.find_feature(feature_id_from_token(token));
  if (!f) {
    return false;
  }
  return content::apply_named_field(&f->fields, field, value);
}

void GisScene::ensure_active_layer() {
  if (impl_->store.find_layer(impl_->store.active_layer_id())) {
    return;
  }
  if (impl_->store.layers().empty()) {
    seed_default();
    return;
  }
  impl_->store.set_active_layer_id(impl_->store.layers().front().id);
}

}  // namespace content

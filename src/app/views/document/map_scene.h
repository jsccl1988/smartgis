// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_MAP_SCENE_H_
#define APP_VIEWS_MAP_SCENE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/catalog_layers.h"
#include "content/public/feature_attrs.h"
#include "content/public/map_types.h"
#include "gis/present/style/style_types.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "gis/vista/world/terrain/land_mask.h"
#include "tool/draft/draft.h"

namespace app {

// In-process map document: layers and features in map space. Pan, scale, and
// paint live on ViewFrame and Map2dPresenter.
class MapScene {
 public:
  // Matches normal map content: area / line / point / annotation text.
  enum class GeomKind { kPoint, kLine, kPolygon, kText };

  // Map-space vertex (OGR / digitize). Not view pixels.
  struct Vertex {
    double x = 0;
    double y = 0;
  };

  using Field = content::NamedField;

  struct Feature {
    content::FeatureId id{};
    GeomKind kind = GeomKind::kPoint;
    std::vector<Vertex> points;
    std::vector<Field> fields;
    bool selected = false;
  };

  struct Layer {
    std::string id;
    std::string name;
    bool visible = true;
    std::vector<Feature> features;
  };

  // Opaque Catalog row; HWND-free wire type lives in content::LayerDesc.
  using LayerDesc = content::LayerDesc;

  MapScene();
  ~MapScene();

  MapScene(const MapScene&) = delete;
  MapScene& operator=(const MapScene&) = delete;

  // Prefer china_city (gpkg/geojson) beside the exe (or testing/data), then
  // schematic china_plp. Multi-layer packs expose area / line / point / text.
  // Falls back to a Demo layer so Catalog/map are never empty.
  void seed_default();

  // Open path via OGR (GPKG / Shapefile / GeoJSON / …). On success replaces
  // document layers with real OGR layer names + geometries (one Catalog
  // layer per OGR layer). Falls back to a tagged sample layer when the file
  // cannot be opened as a vector source. Returns true when at least one OGR
  // feature was ingested.
  bool open_path(const std::string& path);

  // Write the active visible layer to |path| as GeoJSON (OGR "GeoJSON" driver).
  // Falls back to the first visible non-empty layer. Returns false if no
  // features, driver missing, or Create failed. Map-space Y is unflipped to
  // CRS84 lat so reopen via open_path matches ingest.
  bool write_path(const std::string& path) const;

  // Optional MapLibre-subset style. When set, paint uses ResolvedPaint colors
  // for matching source-layer names; otherwise Baidu defaults remain.
  void set_style_document(std::shared_ptr<gis::style::StyleDocument> doc);
  void clear_style_document();
  bool load_style_path(const std::string& path);
  bool has_style_document() const { return static_cast<bool>(style_doc_); }
  const gis::style::StyleDocument* style_document() const {
    return style_doc_.get();
  }

  // Optional XYZ/WMTS TileProvider drawn under vectors (mockable via set_fetch_fn).
  void set_basemap_provider(std::shared_ptr<gis::tile::TileProvider> provider);
  void clear_basemap_provider();
  bool has_basemap_provider() const {
    return basemap_ && basemap_->is_open();
  }

  // Test helper: resolve paint for a Catalog layer name + attrs at |zoom|.
  bool resolve_style_for_test(const std::string& source_layer,
                              const gis::style::AttrMap& attrs, double zoom,
                              gis::style::ResolvedPaint* out) const;

  void clear();

  std::vector<LayerDesc> layer_descs() const;
  const std::string& active_layer_id() const { return active_layer_id_; }
  size_t layer_count() const { return layers_.size(); }
  size_t feature_count() const;
  bool last_open_was_ogr() const { return last_open_was_ogr_; }

  // Envelope of visible feature vertices in map space (Y already flipped for
  // screen). Returns false when there are no vertices.
  bool compute_extent(double* min_x, double* min_y, double* max_x,
                      double* max_y) const;

  // Map-space box for ViewFrame::fit_extent. Prefers polygon rings, then any
  // geometry. Does not apply the China shortcut and does not pad a tiny box.
  // False when there are no vertices.
  bool polygon_fit_box(double* min_x, double* min_y, double* max_x,
                       double* max_y) const;

  const std::vector<Layer>& layers() const { return layers_; }
  gis::tile::TileProvider* basemap_provider() const { return basemap_.get(); }

  // Resolved style colors for one feature. False keeps the cartography defaults.
  bool style_colors_for_feature(const Layer& layer, const Feature& f,
                                double scale, COLORREF* fill, COLORREF* stroke,
                                int* stroke_width) const;

  // True when extent looks like China lon/lat sample (CRS84, Y flipped).
  bool has_china_extent() const;

  bool create_layer(const std::string& name, const std::string& geometry_type);
  bool remove_layer(const std::string& id);
  bool set_layer_visible(const std::string& id, bool visible);
  bool select_layer(const std::string& id);
  bool move_layer(const std::string& id, int delta);

  // Digitize: append into the active visible layer. Returns new feature id.
  // |to_map| converts view pixels to map space. Empty stores x_px/y_px as
  // map coordinates (identity camera).
  content::FeatureId append_from_draft(
      const tool::Draft& draft, const char* tool_id,
      const std::function<void(int view_x, int view_y, double* map_x,
                               double* map_y)>& to_map = {});

  // Move the nearest vertex of the selected feature to the map point.
  // Empty id when nothing is selected or the click misses every vertex.
  content::FeatureId move_selected_vertex(double map_x, double map_y,
                                          double tol_map);

  // Copy map-space XY of one feature. False when the id is unknown.
  bool copy_feature_xy(const content::FeatureId& id,
                       std::vector<std::pair<double, double>>* out) const;

  // Store a TIN/grid footprint as polygon features the 2D map can draw.
  // Returns false when there is no drawable triangle (does not leave an
  // empty layer).
  bool add_triangle_layer(const std::string& name, const double* xyz,
                          int point_count, const int* triangles,
                          int triangle_count);

  // Hit-test in map space. |tol_map| is the caller's tolerance. Selects at
  // most one feature.
  const Feature* hit_test(double map_x, double map_y, double tol_map);
  bool select_feature(const content::FeatureId& id);
  void clear_selection();
  const Feature* selected_feature() const;

  // World lon/lat envelope of the active layer. False when that layer is
  // missing or has no vertices.
  bool active_layer_world_extent(content::Extent2* out) const;
  // World lon/lat envelope of the selected feature. False when none is selected.
  bool selection_world_extent(content::Extent2* out) const;

  // Feature envelope in lon/lat (Y unflipped). China box when empty.
  content::Extent2 world_extent() const;

  // Visible polygon rings in lon/lat (Y unflipped). Used to mask DEM.
  void export_land_rings(std::vector<gis::LonLatRing>* out) const;

  // Inspector helpers (string-only; no leftover GIS pointers).
  // |source_layer| is the Catalog/OGR layer name for style resolution; when
  // empty, the owning layer is inferred from |f.id|. |map_scale| feeds zoom
  // (constant zoom; no interpolate).
  void fill_feature_info_fields(
      const Feature& f,
      std::vector<std::pair<std::string, std::string>>* out,
      const std::string& source_layer = {}, double map_scale = 8.0) const;
  void fill_attribute_rows(std::vector<std::string>* columns,
                           std::vector<std::vector<std::string>>* rows,
                           std::vector<std::string>* tokens) const;

  // Opaque tokens / field apply delegate to content::feature_attrs (no HWND).
  static std::string feature_token(const content::FeatureId& id);
  static content::FeatureId feature_id_from_token(const std::string& token);

  bool update_feature_field(const std::string& token,
                            const std::string& field,
                            const std::string& value);

 private:
  Layer* find_layer(const std::string& id);
  const Layer* find_layer(const std::string& id) const;
  Feature* find_feature(const content::FeatureId& id);
  content::FeatureId next_feature_id();
  void ensure_active_layer();
  void add_sample_features(Layer* layer, const std::string& tag);
  bool ingest_ogr_path(const std::string& path);
  bool try_bootstrap_china_plp();
  // Regroup a single OGR layer into area / line / point / text Catalog layers
  // when features carry a kind= field (china_plp). Skipped when the dataset
  // already has multiple named OGR layers.
  void split_layers_by_kind_field();

  std::vector<Layer> layers_;
  std::string active_layer_id_;
  content::FeatureId selected_id_{};
  uint32_t next_id_ = 1;
  bool last_open_was_ogr_ = false;
  std::shared_ptr<gis::style::StyleDocument> style_doc_;
  std::shared_ptr<gis::tile::TileProvider> basemap_;
};

// Ordered relative paths for default China seed (exe-dir / testing/data).
// china_city packs must precede china_plp so product shells match SmartGis.exe
// prefecture overview rather than the 46-feature schematic PLP.
std::vector<std::string> china_seed_relative_paths();

}  // namespace app

#endif  // APP_VIEWS_MAP_SCENE_H_

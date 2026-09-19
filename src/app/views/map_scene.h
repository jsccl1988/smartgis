// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_MAP_SCENE_H_
#define APP_VIEWS_MAP_SCENE_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "app/views/map_host_extent.h"
#include "content/public/catalog_layers.h"
#include "content/public/feature_attrs.h"
#include "content/public/map_types.h"
#include "gis/world/land_mask.h"
#include "tool/gestures.h"

namespace app {

// In-process map document for product chrome (Views / WinUI / CEF): layers +
// features in map space. GPU still clears/presents a base frame; this scene
// paints vectors (polygon / line / point / annotation text) on top and backs
// Catalog / FeatureInfo / AttributeTable / tools.
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

  // True when extent looks like China lon/lat sample (CRS84, Y flipped).
  bool has_china_extent() const;

  bool create_layer(const std::string& name, const std::string& geometry_type);
  bool remove_layer(const std::string& id);
  bool set_layer_visible(const std::string& id, bool visible);
  bool select_layer(const std::string& id);
  bool move_layer(const std::string& id, int delta);

  // Digitize: append into the active visible layer. Returns new feature id.
  content::FeatureId append_from_draft(const tool::Draft& draft,
                                       const char* tool_id);

  // Hit-test in view pixels (after pan/zoom). Selects at most one feature.
  const Feature* hit_test(int view_x, int view_y, int view_w, int view_h);
  bool select_feature(const content::FeatureId& id);
  void clear_selection();
  const Feature* selected_feature() const;

  void apply_pan(int dx_px, int dy_px);
  void apply_zoom_at(int view_x, int view_y, double factor);
  void apply_pinch(int view_x, int view_y, double scale);
  // Fit all visible feature envelopes into a view of |view_w| x |view_h|.
  void fit_extent(int view_w, int view_h);

  // Current overlay scale (pixels per map unit). Used by --self-test.
  double scale() const { return scale_; }

  // Feature envelope in lon/lat (Y unflipped). China box when empty.
  content::Extent2 world_extent() const;
  // Visible client rect in lon/lat after pan/zoom.
  content::Extent2 view_world_extent(int view_w, int view_h) const;
  // Frame the overlay to a leftover / MapContents lon/lat extent.
  void apply_world_extent(const content::Extent2& e, int view_w, int view_h);

  // Overlay vectors onto the map HWND after GPU present.
  // 2D fills a white SmartGis-like canvas; pass fill_background=false on 3D
  // so DEM / FlyCube frames are not wiped.
  void paint(HDC hdc, int width_px, int height_px) const;
  void paint(HDC hdc, int width_px, int height_px, bool fill_background) const;

  // Visible polygon rings in lon/lat (Y unflipped). Used to mask DEM.
  void export_land_rings(std::vector<gis::LonLatRing>* out) const;

  // Inspector helpers (string-only; no leftover GIS pointers).
  void fill_feature_info_fields(const Feature& f,
                                std::vector<std::pair<std::string, std::string>>*
                                    out) const;
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
  void map_to_view(double mx, double my, int* vx, int* vy) const;
  void view_to_map(int vx, int vy, double* mx, double* my) const;
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
  double pan_x_ = 0;
  double pan_y_ = 0;
  double scale_ = 1.0;
  bool last_open_was_ogr_ = false;
};

// Ordered relative paths for default China seed (exe-dir / testing/data).
// china_city packs must precede china_plp so product shells match SmartGis.exe
// prefecture overview rather than the 46-feature schematic PLP.
std::vector<std::string> china_seed_relative_paths();

// Baidu-like 2D cartography colors used by MapScene::paint (ocean bg,
// warm land wash, soft rivers, light admin strokes / POI discs).
COLORREF map_scene_map_bg_color();
COLORREF map_scene_area_fill_color(const char* adcode, uint32_t feature_id);
COLORREF map_scene_river_color();
COLORREF map_scene_admin_stroke_color();
COLORREF map_scene_point_fill_color();

}  // namespace app

#endif  // APP_VIEWS_MAP_SCENE_H_

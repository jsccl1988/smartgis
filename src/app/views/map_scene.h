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

#include "app/views/map_host_extent.h"
#include "content/public/catalog_layers.h"
#include "content/public/feature_attrs.h"
#include "content/public/map_types.h"
#include "gis/style/style_types.h"
#include "gis/tile/tile_provider.h"
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

  // Optional XYZ/WMTS TileProvider drawn under vectors (mockable via set_fetch_fn).
  void set_basemap_provider(std::shared_ptr<gis::tile::TileProvider> provider);
  void clear_basemap_provider();
  bool has_basemap_provider() const {
    return basemap_ && basemap_->is_open();
  }
  // Tiles drawn on the last paint() call (self-test / unit tests).
  size_t basemap_tiles_drawn() const { return basemap_tiles_drawn_; }

  // Paint to a memory DC and write a 32-bpp BMP (one page export).
  bool export_bmp(const std::string& path, int width_px, int height_px) const;

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

  // Scene3d: place-name labels via a lon/lat→view projector (orbit camera).
  // Stored map Y is -lat; converts before calling |project|. Caps count at
  // country scale so the DEM stays readable (SmartGis.exe SoT parity).
  void paint_labels_projected(
      HDC hdc, int width_px, int height_px,
      const std::function<void(double lon, double lat, int* sx, int* sy)>&
          project) const;

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
  std::shared_ptr<gis::style::StyleDocument> style_doc_;
  std::shared_ptr<gis::tile::TileProvider> basemap_;
  mutable size_t basemap_tiles_drawn_ = 0;

  void paint_basemap_underlay(HDC hdc, int width_px, int height_px) const;
  bool style_colors_for_feature(const Layer& layer, const Feature& f,
                                COLORREF* fill, COLORREF* stroke,
                                int* stroke_width) const;
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
COLORREF map_scene_road_color();
COLORREF map_scene_admin_stroke_color();
COLORREF map_scene_point_fill_color();

// Screen-space label box (right/bottom exclusive), used for collision tests.
struct MapLabelBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

bool map_scene_label_boxes_overlap(MapLabelBox a, MapLabelBox b);
// Greedy in given order: a box that intersects an accepted box is dropped.
size_t map_scene_accept_label_count(const MapLabelBox* boxes, size_t count);

// Country fit on China is about scale 8–16. 3 = province/capital, 2 = city,
// 1 = county or river label, 0 = dense POI.
int map_scene_label_min_importance(double scale);
// Name-only rank (anno/name suffixes and provincial capitals). 0 if unknown.
int map_scene_place_name_importance(const char* utf8_name);

// Water, road, or unclassified line. Generic kind "line" is kOther.
enum class MapLineRole { kWater, kRoad, kOther };

MapLineRole map_scene_line_role(const char* kind, const char* feature_class);
bool map_scene_line_is_major_class(const char* kind, const char* feature_class);
// |length| is map-space polyline length in degrees (see length_as_degrees).
bool map_scene_line_visible_at_scale(MapLineRole role, double length,
                                    bool major_class, double scale);
int map_scene_line_stroke_px(MapLineRole role, double length, double scale);

// One piece of a water/road network. Empty name does not match other names.
struct MapStemSpan {
  const char* name = nullptr;
  double length = 0;
  double x0 = 0;
  double y0 = 0;
  double x1 = 0;
  double y1 = 0;
};

// Component length of spans[index]. Same non-empty name joins a stem.
// Touching endpoints join only when the names are not two different labels,
// so a named tributary does not swallow the trunk it meets.
double map_scene_stem_length(const MapStemSpan* spans, size_t count,
                             size_t index, double touch_tol);

// Mid-length point and tangent of a polyline (y-down atan2, folded to
// [-90, 90] so glyphs stay upright). Not the vertex centroid.
struct MapLineLabelAnchor {
  double x = 0;
  double y = 0;
  double angle_deg = 0;
  bool ok = false;
};

MapLineLabelAnchor map_scene_line_label_anchor(const double* xs,
                                              const double* ys, size_t count);

// True when the bbox fits a lon/lat frame. Projected meter windows do not.
bool map_scene_extent_is_lonlat(double minx, double miny, double maxx,
                               double maxy);
// Meters become degrees (~111320 m). Lon/lat lengths are unchanged.
double map_scene_length_as_degrees(double length, bool lonlat);

}  // namespace app

#endif  // APP_VIEWS_MAP_SCENE_H_

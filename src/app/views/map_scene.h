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

#include "content/public/map_types.h"
#include "tool/gestures.h"
#include "ui/views/layer_tree.h"

namespace app {

// In-process map document for Views chrome: layers + features in map space.
// GPU still clears/presents a base frame; this scene paints vectors on top and
// backs Catalog / FeatureInfo / AttributeTable / select-draw tools.
class MapScene {
 public:
  enum class GeomKind { kPoint, kLine, kPolygon };

  // Map-space vertex (OGR / digitize). Not view pixels.
  struct Vertex {
    double x = 0;
    double y = 0;
  };

  struct Field {
    std::string name;
    std::string value;
  };

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

  MapScene();

  // Seed Demo layer + sample geometries so Catalog/map are never empty.
  void seed_default();

  // Open path via OGR (GPKG / Shapefile / GeoJSON / …). On success replaces
  // document layers with real OGR layer names + geometries. Falls back to a
  // tagged sample layer when the file cannot be opened as a vector source.
  // Returns true when at least one OGR feature was ingested.
  bool open_path(const std::string& path);

  void clear();

  std::vector<ui::views::LayerTree::LayerDesc> layer_descs() const;
  const std::string& active_layer_id() const { return active_layer_id_; }
  size_t layer_count() const { return layers_.size(); }
  size_t feature_count() const;
  bool last_open_was_ogr() const { return last_open_was_ogr_; }

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
  // Fit all visible feature envelopes into a view of |view_w| x |view_h|.
  void fit_extent(int view_w, int view_h);

  // Overlay vectors onto the map HWND after GPU present.
  void paint(HDC hdc, int width_px, int height_px) const;

  // Inspector helpers (string-only; no leftover GIS pointers).
  void fill_feature_info_fields(const Feature& f,
                                std::vector<std::pair<std::string, std::string>>*
                                    out) const;
  void fill_attribute_rows(std::vector<std::string>* columns,
                           std::vector<std::vector<std::string>>* rows,
                           std::vector<std::string>* tokens) const;

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

  std::vector<Layer> layers_;
  std::string active_layer_id_;
  content::FeatureId selected_id_{};
  uint32_t next_id_ = 1;
  double pan_x_ = 0;
  double pan_y_ = 0;
  double scale_ = 1.0;
  bool last_open_was_ogr_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_MAP_SCENE_H_

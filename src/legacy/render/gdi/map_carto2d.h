// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_GDI_MAP_CARTO2D_H_
#define SMT_LEGACY_RENDER_GDI_MAP_CARTO2D_H_

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace render {

// Screen-space label box used by 2D GDI declutter.
struct MapCartoBox {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  int priority = 0;
};

// Lower score wins (title/province before county).
int carto2d_label_priority(const char* name, const char* kind, const char* cls,
                           const char* adcode);

// Max accepted priority at this map scale (fblc = device px per map unit).
int carto2d_lod_max_priority(float fblc);

int carto2d_label_budget(float fblc);
int carto2d_point_min_distance(float fblc);
int carto2d_utf8_units(const char* text);

// Screen-pixel style at |fblc|. Used to override washed OGR defaults.
int carto2d_label_px(int priority, float fblc);
int carto2d_halo_px(int priority);
int carto2d_point_radius(float fblc);
int carto2d_stroke_px(float fblc, bool river);
int carto2d_stroke_px_kind(float fblc, bool river, bool road);
// Baidu-like cartography. Unique-value / choropleth input is ignored.
unsigned carto2d_map_bg();
unsigned carto2d_land_fill();
unsigned carto2d_water_fill();
unsigned carto2d_boost_fill(unsigned bgr);
unsigned carto2d_admin_stroke();
unsigned carto2d_river_color();
unsigned carto2d_road_color();
bool carto2d_is_river_kind(const char* kind);
bool carto2d_is_road_kind(const char* kind, const char* cls = nullptr);

// 0=none, 1=road, 2=highway, 3=expressway (from kind and/or cls tags).
int carto2d_road_class(const char* kind, const char* cls);
unsigned carto2d_road_casing_color(int road_class);
unsigned carto2d_road_fill_color(int road_class);
int carto2d_road_width_px(float fblc, int road_class);

// First non-empty annotation field (anno, name, text).
const char* carto2d_label_text(const char* anno, const char* name,
                               const char* text);

MapCartoBox carto2d_label_box(int x, int y, const char* text, int px_h,
                              int priority);
MapCartoBox carto2d_label_box_rotated(int x, int y, const char* text,
                                      int px_h, int priority,
                                      float angle_deg);

// Screen pose for a line-following label (mid segment, upright angle).
struct MapCartoLineLabel {
  int x = 0;
  int y = 0;
  float angle_deg = 0.f;
};
bool carto2d_line_label_pose(const int* xy_pairs, int n_pts,
                             MapCartoLineLabel* out);

bool carto2d_boxes_overlap(const MapCartoBox& a, const MapCartoBox& b);

// Per-frame occupancy for streaming GDI paint (labels + point symbols).
class MapCarto2dFrame {
 public:
  void reset(float fblc, int view_w, int view_h);
  bool try_keep_label(const MapCartoBox& box);
  bool try_keep_point(int x, int y);
  int label_count() const { return static_cast<int>(labels_.size()); }
  int point_count() const;

 private:
  bool box_in_view(const MapCartoBox& box) const;
  static int64_t label_grid_key(int cell_x, int cell_y);
  void label_grid_insert(size_t label_index, const MapCartoBox& box,
                         int cell_size);
  bool label_overlaps_kept(const MapCartoBox& box, int cell_size) const;

  float fblc_ = 1.f;
  int view_w_ = 800;
  int view_h_ = 600;
  int max_priority_ = 9;
  int budget_ = 200;
  int min_dist_ = 0;
  std::vector<MapCartoBox> labels_;
  std::unordered_map<int64_t, std::vector<size_t>> label_grid_;
  std::vector<int> points_;
};

}  // namespace render

#endif  // SMT_LEGACY_RENDER_GDI_MAP_CARTO2D_H_

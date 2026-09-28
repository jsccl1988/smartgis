// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_CARTO_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_CARTO_H_

#include <cstddef>
#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {

// Baidu-like 2D cartography colors used by Map2dPresenter::paint (ocean bg,
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
// Prefer map_scene_fill_stem_lengths when many indices are needed (paint);
// calling this once per span is O(n^3) and freezes china_city.
double map_scene_stem_length(const MapStemSpan* spans, size_t count,
                             size_t index, double touch_tol);

// One Union-Find pass: out_lengths[i] is the stem length for spans[i].
// |out_lengths| must hold |count| doubles. O(n^2) endpoint checks.
void map_scene_fill_stem_lengths(const MapStemSpan* spans, size_t count,
                                 double touch_tol, double* out_lengths);

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

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_CARTO_H_

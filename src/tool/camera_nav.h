// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_CAMERA_NAV_H_
#define TOOL_CAMERA_NAV_H_

#include <cstdint>

namespace tool {

// Industry-map camera math (Baidu / Google 2D, Cesium-style 3D). Pixel space
// only — no leftover render types. Wheel / pinch stay in client coordinates.

// WHEEL_DELTA (120) → multiplicative scale. Positive wheel zooms in.
double wheel_zoom_factor(int32_t wheel_delta);

// WM_MOUSEHWHEEL / trackpad horizontal delta → pan pixels (screen X).
// Positive wheel (scroll right) pans content right.
int32_t hwheel_pan_dx(int32_t wheel_delta);

// Convert two finger-pixel distances into a WHEEL_DELTA-class step so leftover
// and chrome hosts share one zoom path. Distances <= 1 px are rejected.
int32_t pinch_to_wheel_delta(double dist0, double dist1);

// Tracker / gesture `scale` is new/old distance (1.2 = pinch-out). Maps that
// ratio onto pinch_to_wheel_delta without hitting the 1 px guard.
int32_t scale_to_wheel_delta(double scale);

// Keep the map point under (cursor_x, cursor_y) fixed while scale changes.
void zoom_at_client_point(double* pan_x, double* pan_y, double* scale,
                          int cursor_x, int cursor_y, double factor);

// StretchBlt dest in view pixels. Zoom origin is the cursor (industry GIS).
struct BlitDestRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

// World envelope (minx/miny/maxx/maxy). Screen Y is down; world Y is up.
struct WorldExtent {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
};

// Idle ms after the last wheel/pan tick before a full vector redraw.
constexpr int kBlitDebounceMs = 200;

// Last-frame StretchBlt dest so the world point under the cursor stays put.
// |factor| > 1 zooms in (dest grows around the cursor).
BlitDestRect zoom_blit_dest(int view_w, int view_h, int cursor_x, int cursor_y,
                            double factor);

// New world envelope after zoom-to-cursor (same contract as apply_zoom_at).
WorldExtent zoom_world_extent(const WorldExtent& last, int view_w, int view_h,
                              int cursor_x, int cursor_y, double factor);

BlitDestRect pan_blit_dest(int view_w, int view_h, int dx_px, int dy_px);
WorldExtent pan_world_extent(const WorldExtent& last, int view_w, int view_h,
                             int dx_px, int dy_px);

// Cesium-style orbit: left/right drag yaw, up/down pitch (radians).
void orbit_from_drag(float* yaw, float* pitch, int dx_px, int dy_px,
                     float sensitivity);

// Dolly along the look vector. Positive wheel → closer to look-at.
float dolly_distance(float distance, int32_t wheel_delta, float min_d,
                     float max_d);

// True when the active tool id is a navigate / view-control tool (or empty).
// Drawing / select tools keep exclusive pointer capture.
bool is_navigate_tool(const char* tool_id);

// Two-finger pinch tracker. Client pixels. scale_out is new/old distance.
class PointerPinchTracker {
 public:
  void reset();
  void on_down(uint32_t id, int x, int y);
  // Returns true when two contacts moved and |scale_out| was written.
  bool on_move(uint32_t id, int x, int y, double* scale_out);
  void on_up(uint32_t id);
  int contact_count() const;

 private:
  struct Contact {
    uint32_t id = 0;
    int x = 0;
    int y = 0;
    bool live = false;
  };
  Contact a_{};
  Contact b_{};
  double last_dist_ = 0;
};

}  // namespace tool

#endif  // TOOL_CAMERA_NAV_H_

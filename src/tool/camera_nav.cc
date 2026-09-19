// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/camera_nav.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tool {
namespace {

constexpr double kStep = 1.1;
constexpr int32_t kWheelUnit = 120;

double hypot_px(int ax, int ay, int bx, int by) {
  const double dx = static_cast<double>(ax - bx);
  const double dy = static_cast<double>(ay - by);
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

double wheel_zoom_factor(int32_t wheel_delta) {
  if (wheel_delta == 0) {
    return 1.0;
  }
  const double steps = static_cast<double>(wheel_delta) / kWheelUnit;
  return std::pow(kStep, steps);
}

int32_t hwheel_pan_dx(int32_t wheel_delta) {
  // Precision touchpads send small deltas; notches send ±120. Use the raw
  // value as pixels so one notch ≈ 120 px and smooth scroll stays proportional.
  return wheel_delta;
}

int32_t pinch_to_wheel_delta(double dist0, double dist1) {
  if (dist0 <= 1.0 || dist1 <= 1.0) {
    return 0;
  }
  const double ratio = dist1 / dist0;
  if (ratio > 0.995 && ratio < 1.005) {
    return 0;
  }
  const double steps = std::log(ratio) / std::log(kStep);
  const int32_t delta = static_cast<int32_t>(std::lround(steps * kWheelUnit));
  if (delta == 0) {
    return ratio > 1.0 ? kWheelUnit : -kWheelUnit;
  }
  return delta;
}

int32_t scale_to_wheel_delta(double scale) {
  if (!(scale > 0.0)) {
    return 0;
  }
  return pinch_to_wheel_delta(100.0, 100.0 * scale);
}

void zoom_at_client_point(double* pan_x, double* pan_y, double* scale,
                          int cursor_x, int cursor_y, double factor) {
  if (!pan_x || !pan_y || !scale || factor <= 0.0) {
    return;
  }
  const double mx = (static_cast<double>(cursor_x) - *pan_x) / *scale;
  const double my = (static_cast<double>(cursor_y) - *pan_y) / *scale;
  *scale *= factor;
  if (*scale < 0.0001) {
    *scale = 0.0001;
  }
  if (*scale > 1.0e6) {
    *scale = 1.0e6;
  }
  *pan_x = static_cast<double>(cursor_x) - mx * *scale;
  *pan_y = static_cast<double>(cursor_y) - my * *scale;
}

BlitDestRect zoom_blit_dest(int view_w, int view_h, int cursor_x, int cursor_y,
                            double factor) {
  BlitDestRect d;
  if (view_w <= 0 || view_h <= 0 || !(factor > 0.0)) {
    return d;
  }
  d.w = static_cast<int>(std::lround(static_cast<double>(view_w) * factor));
  d.h = static_cast<int>(std::lround(static_cast<double>(view_h) * factor));
  if (d.w < 1) {
    d.w = 1;
  }
  if (d.h < 1) {
    d.h = 1;
  }
  d.x = static_cast<int>(
      std::lround(static_cast<double>(cursor_x) * (1.0 - factor)));
  d.y = static_cast<int>(
      std::lround(static_cast<double>(cursor_y) * (1.0 - factor)));
  return d;
}

WorldExtent zoom_world_extent(const WorldExtent& last, int view_w, int view_h,
                              int cursor_x, int cursor_y, double factor) {
  WorldExtent out = last;
  if (view_w <= 0 || view_h <= 0 || !(factor > 0.0)) {
    return out;
  }
  const double span_x = last.maxx - last.minx;
  const double span_y = last.maxy - last.miny;
  const double u = static_cast<double>(cursor_x) / static_cast<double>(view_w);
  const double v = static_cast<double>(cursor_y) / static_cast<double>(view_h);
  const double wx = last.minx + u * span_x;
  const double wy = last.maxy - v * span_y;
  const double nx = span_x / factor;
  const double ny = span_y / factor;
  out.minx = wx - u * nx;
  out.maxx = out.minx + nx;
  out.maxy = wy + v * ny;
  out.miny = out.maxy - ny;
  return out;
}

BlitDestRect pan_blit_dest(int view_w, int view_h, int dx_px, int dy_px) {
  BlitDestRect d;
  d.x = dx_px;
  d.y = dy_px;
  d.w = view_w > 0 ? view_w : 0;
  d.h = view_h > 0 ? view_h : 0;
  return d;
}

WorldExtent pan_world_extent(const WorldExtent& last, int view_w, int view_h,
                             int dx_px, int dy_px) {
  WorldExtent out = last;
  if (view_w <= 0 || view_h <= 0) {
    return out;
  }
  const double span_x = last.maxx - last.minx;
  const double span_y = last.maxy - last.miny;
  const double dwx = static_cast<double>(dx_px) * span_x /
                     static_cast<double>(view_w);
  const double dwy = static_cast<double>(dy_px) * span_y /
                     static_cast<double>(view_h);
  // Screen Y down: dragging content down (+dy) shows world farther north.
  out.minx -= dwx;
  out.maxx -= dwx;
  out.miny += dwy;
  out.maxy += dwy;
  return out;
}

void orbit_from_drag(float* yaw, float* pitch, int dx_px, int dy_px,
                     float sensitivity) {
  if (!yaw || !pitch) {
    return;
  }
  const float s = sensitivity > 0.f ? sensitivity : 0.01f;
  *yaw += static_cast<float>(dx_px) * s;
  *pitch += static_cast<float>(dy_px) * s;
  *pitch = std::clamp(*pitch, -1.2f, 1.2f);
}

float dolly_distance(float distance, int32_t wheel_delta, float min_d,
                     float max_d) {
  const double f = wheel_zoom_factor(wheel_delta);
  // Positive wheel → closer (smaller distance).
  float next = static_cast<float>(static_cast<double>(distance) / f);
  if (next < min_d) {
    next = min_d;
  }
  if (next > max_d) {
    next = max_d;
  }
  return next;
}

bool is_navigate_tool(const char* tool_id) {
  if (!tool_id || !tool_id[0]) {
    return true;
  }
  if (std::strncmp(tool_id, "view.", 5) == 0) {
    return true;
  }
  if (std::strncmp(tool_id, "view3d.", 7) == 0) {
    return true;
  }
  if (std::strcmp(tool_id, "wheel.zoom") == 0) {
    return true;
  }
  return false;
}

void PointerPinchTracker::reset() {
  a_ = {};
  b_ = {};
  last_dist_ = 0;
}

void PointerPinchTracker::on_down(uint32_t id, int x, int y) {
  Contact* slot = nullptr;
  if (!a_.live || a_.id == id) {
    slot = &a_;
  } else if (!b_.live || b_.id == id) {
    slot = &b_;
  } else {
    return;
  }
  slot->id = id;
  slot->x = x;
  slot->y = y;
  slot->live = true;
  last_dist_ = (a_.live && b_.live) ? hypot_px(a_.x, a_.y, b_.x, b_.y) : 0;
}

bool PointerPinchTracker::on_move(uint32_t id, int x, int y, double* scale_out) {
  Contact* slot = nullptr;
  if (a_.live && a_.id == id) {
    slot = &a_;
  } else if (b_.live && b_.id == id) {
    slot = &b_;
  } else {
    return false;
  }
  slot->x = x;
  slot->y = y;
  if (!a_.live || !b_.live || !scale_out) {
    return false;
  }
  const double d = hypot_px(a_.x, a_.y, b_.x, b_.y);
  if (last_dist_ <= 1.0 || d <= 1.0) {
    last_dist_ = d;
    return false;
  }
  *scale_out = d / last_dist_;
  last_dist_ = d;
  return true;
}

void PointerPinchTracker::on_up(uint32_t id) {
  if (a_.live && a_.id == id) {
    a_ = {};
  } else if (b_.live && b_.id == id) {
    b_ = {};
  }
  last_dist_ = 0;
}

int PointerPinchTracker::contact_count() const {
  return (a_.live ? 1 : 0) + (b_.live ? 1 : 0);
}

}  // namespace tool

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/camera/orbit_frame.h"

#include <algorithm>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "tool/nav/camera_nav.h"

namespace app {

OrbitFrame::OrbitFrame() {
  extent_ = kChinaLonLatExtent;
}

content::Extent2 OrbitFrame::world_extent() const {
  return china_or(extent_);
}

void OrbitFrame::apply_world_extent(const content::Extent2& e) {
  if (!extent_nonempty(e)) {
    return;
  }
  extent_ = e;
}

void OrbitFrame::remember_view_size(int width_px, int height_px) {
  if (width_px > 0) {
    last_w_ = width_px;
  }
  if (height_px > 0) {
    last_h_ = height_px;
  }
}

void OrbitFrame::apply_wheel_at(int view_x, int view_y, int32_t wheel,
                                int view_w, int view_h) {
  remember_view_size(view_w, view_h);
  const float w = view_w > 0 ? static_cast<float>(view_w) : 1.f;
  const float h = view_h > 0 ? static_cast<float>(view_h) : 1.f;
  const float nx = 2.f * static_cast<float>(view_x) / w - 1.f;
  const float ny = 1.f - 2.f * static_cast<float>(view_y) / h;
  const double factor = tool::wheel_zoom_factor(wheel);
  const float pull = static_cast<float>(1.0 - 1.0 / factor);
  yaw_ += nx * 0.12f * pull;
  pitch_ += ny * 0.08f * pull;
  pitch_ = std::clamp(pitch_, tool::kOrbitPitchMin, tool::kOrbitPitchMax);
  distance_ = tool::dolly_distance(distance_, wheel, 1.2f, 12.f);
}

void OrbitFrame::apply_pan(int dx_px, int dy_px) {
  // Horizontal pan orbits yaw. Vertical pan dollies (no pitch — edge-on
  // pitch collapsed the DEM to a strip). Scale the dolly by pixels: one
  // wheel notch per ~80px. A raw ±120 per move made a short drag fly into
  // the mesh and left browse stuck on the clipped face.
  yaw_ += static_cast<float>(dx_px) * 0.008f;
  if (dy_px != 0) {
    const int mag = dy_px < 0 ? -dy_px : dy_px;
    const int sign = dy_px > 0 ? -1 : 1;
    int32_t wheel = sign * (mag * 120) / 80;
    if (wheel == 0) {
      wheel = sign;
    }
    distance_ = tool::dolly_distance(distance_, wheel, 1.2f, 12.f);
  }
}

void OrbitFrame::apply_pinch(int view_x, int view_y, double scale, int view_w,
                             int view_h) {
  const int32_t wheel = tool::scale_to_wheel_delta(scale);
  if (wheel == 0) {
    return;
  }
  apply_wheel_at(view_x, view_y, wheel, view_w, view_h);
}

bool OrbitFrame::apply_nav_key(uint32_t key) {
  uint32_t k = key;
  if (k >= 'a' && k <= 'z') {
    k = k - ('a' - 'A');
  }
  constexpr float kYawStep = 0.08f;
  switch (k) {
    case 'W':
    case VK_UP:
      distance_ = tool::dolly_distance(distance_, 120, 1.2f, 12.f);
      return true;
    case 'S':
    case VK_DOWN:
      distance_ = tool::dolly_distance(distance_, -120, 1.2f, 12.f);
      return true;
    case 'A':
    case VK_LEFT:
      yaw_ -= kYawStep;
      return true;
    case 'D':
    case VK_RIGHT:
      yaw_ += kYawStep;
      return true;
    default:
      return false;
  }
}

void OrbitFrame::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kKey) {
    apply_nav_key(draft.key);
    return;
  }

  if (draft.kind == tool::DraftKind::kWheel) {
    if (!draft.points.empty() && last_w_ > 0 && last_h_ > 0) {
      apply_wheel_at(draft.points.front().x_px, draft.points.front().y_px,
                     draft.wheel, last_w_, last_h_);
    } else {
      distance_ = tool::dolly_distance(distance_, draft.wheel, 1.2f, 12.f);
    }
    return;
  }

  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    const int dx = draft.points[1].x_px - draft.points[0].x_px;
    const int dy = draft.points[1].y_px - draft.points[0].y_px;
    const bool orbit = (draft.flags & (MK_RBUTTON | MK_MBUTTON)) != 0;
    if (orbit) {
      tool::orbit_from_drag(&yaw_, &pitch_, dx, dy, 0.01f);
    } else {
      apply_pan(dx, dy);
    }
    last_x_ = draft.points[1].x_px;
    last_y_ = draft.points[1].y_px;
    has_last_ = true;
    return;
  }

  if (draft.points.empty()) {
    return;
  }

  if (draft.kind == tool::DraftKind::kPoint) {
    const int x = draft.points.front().x_px;
    const int y = draft.points.front().y_px;
    if (has_last_) {
      tool::orbit_from_drag(&yaw_, &pitch_, x - last_x_, y - last_y_, 0.01f);
    }
    last_x_ = x;
    last_y_ = y;
    has_last_ = true;
  }
}

render::rhi::CameraMatrices OrbitFrame::camera_matrices(float aspect) const {
  return render::rhi::make_orbit_camera(yaw_, pitch_, distance_, kScene3dFovY,
                                        aspect, 0.1f, 100.f);
}

render::rhi::CameraMatrices OrbitFrame::camera_matrices_ortho(
    float width_px, float height_px) const {
  (void)width_px;
  (void)height_px;
  const content::Extent2 e = world_extent();
  return render::rhi::make_ortho_camera(
      static_cast<float>(e.xmin), static_cast<float>(e.xmax),
      static_cast<float>(e.ymin), static_cast<float>(e.ymax), -1.f, 1.f);
}

void OrbitFrame::project(float x, float y, float z, int width_px, int height_px,
                         int* sx, int* sy) const {
  const float cy = std::cos(yaw_);
  const float syaw = std::sin(yaw_);
  const float cp = std::cos(pitch_);
  const float sp = std::sin(pitch_);
  const float x1 = x * cy - z * syaw;
  const float z1 = x * syaw + z * cy;
  const float y2 = y * cp - z1 * sp;
  const float z2 = y * sp + z1 * cp;
  const float depth = z2 + distance_;
  const float inv = depth > 0.15f ? (1.f / depth) : (1.f / 0.15f);
  const float f = 280.f * inv;
  if (sx) {
    *sx = width_px / 2 + static_cast<int>(std::lround(x1 * f));
  }
  if (sy) {
    *sy = height_px / 2 - static_cast<int>(std::lround(y2 * f));
  }
}

void OrbitFrame::project_lon_lat(double lon, double lat, int width_px,
                                 int height_px, int* sx, int* sy) const {
  // Match DEM mesh framing: geographic X=-lon, Z=lat, then the same
  // center/scale as normalize_mesh over the active world extent.
  const content::Extent2 e = world_extent();
  const float minx = gis::dem_lon_to_x(e.xmax);  // xmax lon → more negative X
  const float maxx = gis::dem_lon_to_x(e.xmin);
  const float minz = static_cast<float>(e.ymin);
  const float maxz = static_cast<float>(e.ymax);
  const float cx = 0.5f * (minx + maxx);
  const float cz = 0.5f * (minz + maxz);
  const float span = (std::max)(maxx - minx, (std::max)(maxz - minz, 1.f));
  const float s = 3.2f / span;
  const float x = (gis::dem_lon_to_x(lon) - cx) * s;
  const float z = (static_cast<float>(lat) - cz) * s;
  project(x, 0.f, z, width_px, height_px, sx, sy);
}

void OrbitFrame::reset() {
  yaw_ = kScene3dDefaultYaw;
  pitch_ = 0.4f;
  distance_ = 3.2f;
  has_last_ = false;
}

}  // namespace app

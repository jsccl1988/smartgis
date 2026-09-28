// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAMERA_ORBIT_FRAME_H_
#define CONTENT_BROWSER_CAMERA_ORBIT_FRAME_H_

#include <cstdint>

#include "content/browser/camera/map_host_extent.h"
#include "gis/vista/world/terrain/dem_frame.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"

namespace content {

// Alias of gis::kDemDefaultOrbitYaw (south-of-target, north toward screen top).
inline constexpr float kScene3dDefaultYaw = gis::kDemDefaultOrbitYaw;

// Vertical FOV for the orbit camera. The DEM is normalized to a 3.2 span
// (XZ diagonal ~4.5) and the default distance is 3.2. 45° only covers ~2.65
// world units at that distance, so FlyCube clipped the country to a patch.
// ~77° fits the diagonal with margin for the pitched ground plane.
inline constexpr float kScene3dFovY = 1.35f;

// Orbit camera for a 3D view: yaw, pitch, distance, and the lon/lat extent
// used for ortho framing and geographic projection.
class OrbitFrame {
 public:
  OrbitFrame();

  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float distance() const { return distance_; }

  const content::Extent2& extent() const { return extent_; }
  void set_extent(const content::Extent2& e) { extent_ = e; }

  content::Extent2 world_extent() const;
  void apply_world_extent(const content::Extent2& e);

  void remember_view_size(int width_px, int height_px);
  int last_width() const { return last_w_; }
  int last_height() const { return last_h_; }

  void apply_wheel_at(int view_x, int view_y, int32_t wheel, int view_w,
                      int view_h);
  void apply_pan(int dx_px, int dy_px);
  void apply_pinch(int view_x, int view_y, double scale, int view_w,
                   int view_h);

  // W/S/A/D and arrow keys only. Every other key, including K and J, returns
  // false and leaves the camera unchanged.
  bool apply_nav_key(uint32_t key);
  void apply_draft(const tool::Draft& draft);

  render::rhi::CameraMatrices camera_matrices(float aspect) const;
  render::rhi::CameraMatrices camera_matrices_ortho(float width_px,
                                                    float height_px) const;

  void project(float x, float y, float z, int width_px, int height_px, int* sx,
               int* sy) const;
  void project_lon_lat(double lon, double lat, int width_px, int height_px,
                       int* sx, int* sy) const;

  // Restores yaw, pitch, distance, and the drag anchor. Extent and the last
  // view size stay as they are.
  void reset();

 private:
  content::Extent2 extent_{};
  float yaw_ = kScene3dDefaultYaw;
  float pitch_ = 0.4f;
  float distance_ = 3.2f;
  int last_x_ = 0;
  int last_y_ = 0;
  int last_w_ = 0;
  int last_h_ = 0;
  bool has_last_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAMERA_ORBIT_FRAME_H_

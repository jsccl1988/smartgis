// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAMERA_ORBIT_FRAME_H_
#define CONTENT_BROWSER_CAMERA_ORBIT_FRAME_H_

#include <cstdint>

#include "content/browser/camera/gis_host_extent.h"
#include "vista/terrain/dem/dem_frame.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"

namespace content {

// Alias of vista::kDemDefaultOrbitYaw (south-of-target, north toward screen top).
inline constexpr float kScene3dDefaultYaw = vista::kDemDefaultOrbitYaw;

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

  // Dolly distance in world units (clamped to the wheel / dolly range).
  void set_distance(float distance);

  // Override dolly clamp (default [1.2, 12]). Globe skim uses a lower floor
  // so the camera can hug DEM relief without piercing peaks.
  void set_dolly_limits(float min_distance, float max_distance);

  // Orbit yaw in radians (around +Y).
  void set_yaw(float yaw);

  // Orbit pitch in radians (clamped to tool::kOrbitPitchMin/Max).
  void set_pitch(float pitch);

  // Globe terrain-hug: horizontal forward look-at (eye above DEM, target ahead
  // on the flight path). When active, camera_matrices ignores orbit look-at-origin.
  void set_forward_skim(float eye_x, float eye_y, float eye_z, float target_x,
                        float target_y, float target_z);
  void clear_forward_skim();
  bool forward_skim_active() const { return forward_skim_; }

 private:
  content::Extent2 extent_{};
  float yaw_ = kScene3dDefaultYaw;
  float pitch_ = 0.4f;
  float distance_ = 3.2f;
  float dolly_min_ = 1.2f;
  float dolly_max_ = 12.f;
  int last_x_ = 0;
  int last_y_ = 0;
  int last_w_ = 0;
  int last_h_ = 0;
  bool has_last_ = false;
  bool forward_skim_ = false;
  float skim_eye_x_ = 0.f;
  float skim_eye_y_ = 0.f;
  float skim_eye_z_ = 0.f;
  float skim_tgt_x_ = 0.f;
  float skim_tgt_y_ = 0.f;
  float skim_tgt_z_ = 0.f;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAMERA_ORBIT_FRAME_H_

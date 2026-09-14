// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SCENE3D_CONTROLLER_H_
#define APP_VIEWS_SCENE3D_CONTROLLER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "render/rhi/rhi.h"
#include "tool/gestures.h"

namespace app {

// Chrome-side 3D camera + demo mesh. Trackball / wheel update yaw/pitch/
// distance; when a FlyCube Device is hung, present_gpu feeds the orbit camera
// into a solid cube draw so the 3D tab is not a static clear.
class Scene3dController {
 public:
  Scene3dController();
  ~Scene3dController();

  Scene3dController(const Scene3dController&) = delete;
  Scene3dController& operator=(const Scene3dController&) = delete;

  void reset();
  void apply_draft(const tool::Draft& draft);

  // Drop GPU mesh pointers without destroying buffers (Device owns them /
  // is shutting down). Call before MapViewport::detach / Device::shutdown.
  void abandon_mesh();

  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float distance() const { return distance_; }

  // Orbit camera matrices for the current yaw / pitch / distance.
  render::rhi::CameraMatrices camera_matrices(float aspect) const;

  // Draw ground grid + cube into |hdc| (view pixels). Used when no GPU device.
  void paint(HDC hdc, int width_px, int height_px) const;
  // Status text only (transparent) for overlay on top of FlyCube present.
  void paint_hud(HDC hdc, int width_px, int height_px) const;

  // Record cube + orbit camera on |device|, execute, and present. Returns true
  // when a GPU frame was submitted.
  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px);

 private:
  void project(float x, float y, float z, int width_px, int height_px, int* sx,
               int* sy) const;
  void release_mesh();
  bool ensure_cube_mesh(render::rhi::Device* device);

  float yaw_ = 0.55f;
  float pitch_ = 0.4f;
  float distance_ = 3.2f;
  int last_x_ = 0;
  int last_y_ = 0;
  bool has_last_ = false;

  render::rhi::Device* mesh_device_ = nullptr;
  render::rhi::Buffer* cube_vb_ = nullptr;
  render::rhi::Buffer* cube_ib_ = nullptr;
  uint32_t cube_index_count_ = 0;
};

}  // namespace app

#endif  // APP_VIEWS_SCENE3D_CONTROLLER_H_

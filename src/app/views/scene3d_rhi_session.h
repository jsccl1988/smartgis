// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SCENE3D_RHI_SESSION_H_
#define APP_VIEWS_SCENE3D_RHI_SESSION_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace render {
namespace rhi {
class Device;
}
}  // namespace render

namespace app {

class Scene3dController;

// True when SMT_FORCE_CONTENT_MAPVIEW_3D=1, or legacy SMT_PREFER_FLYCUBE_3D=0.
// Opt out of the default Scene3d FlyCube / present_gpu path (DX12 hang hosts).
bool force_content_mapview_3d();

// Prefer FlyCube RHI for kScene3d unless force_content_mapview_3d().
bool prefer_scene3d_flycube();

// Owns a FlyCube (or preferred GPU) RHI device bound to an HWND for
// Scene3dController::present_gpu. GDI paint remains the fallback when attach
// or present fails.
class Scene3dRhiSession {
 public:
  Scene3dRhiSession() = default;
  ~Scene3dRhiSession();

  Scene3dRhiSession(const Scene3dRhiSession&) = delete;
  Scene3dRhiSession& operator=(const Scene3dRhiSession&) = delete;

  bool try_attach(HWND hwnd);
  void release();
  bool is_live() const { return device_ != nullptr; }
  render::rhi::Device* device() const { return device_; }

  // Calls Scene3dController::present_gpu and records last_present_ok().
  bool present(Scene3dController* cam, uint32_t width_px, uint32_t height_px);
  bool last_present_ok() const { return last_ok_; }

  // Re-initialize the swapchain after HWND resize (FlyCube DeviceDesc).
  void resize(HWND hwnd, uint32_t width_px, uint32_t height_px);

 private:
  render::rhi::Device* device_ = nullptr;
  bool last_ok_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_SCENE3D_RHI_SESSION_H_

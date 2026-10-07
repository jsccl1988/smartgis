// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_SESSION_DEVICE_SESSION_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_SESSION_DEVICE_SESSION_H_

#include <windows.h>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace plugin {

class HarnessShell;

namespace detail {

// Policy for Scene3D plugin showcase HWND + RHI device setup.
struct PluginDeviceSessionOpts {
  // Mode-specific GPU env (e.g. PLUGIN_MINE_GPU). Falls back to
  // PLUGIN_WORLD3D_GPU, then default-on when unset.
  const char* gpu_env = nullptr;
  // When true, require a live scene_draw_host HWND before detach/create.
  bool require_scene_hwnd = true;
  // Drop ContentMapView / FlyCube on the tab before owning a present HWND.
  bool detach_flycube = true;
  // Clear + present once so FlyCube first frame is not an empty backbuffer.
  bool warm_swapchain = false;
  // Stormsurge Null path: skip borrowed HWND (writers + orbit only).
  bool allow_null_without_hwnd = false;
  // When false, attach DrawHost Role::kScene3d without select_view_tab(1)
  // (that switch AVs on the GDI/lazy ContentMapView path; peer atmosphere).
  // Mapped to RhiPresentSessionOpts.select_scene_tab; borrow is always on.
  bool borrow_shell_scene3d = true;
};

// Owned / borrowed RHI present surface for product plugin Scene3D bodies.
struct PluginDeviceSession {
  render::rhi::Device* device = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool want_gpu = false;
  bool borrowed_shell = false;
};

// Creates present HWND (GPU) or borrows tab child (Null), then initializes
// Device. On failure returns non-zero and leaves |out| partially filled so the
// caller can DestroyWindow / detach_views.
int prepare_plugin_device_session(HarnessShell& browser,
                                  const PluginDeviceSessionOpts& opts,
                                  PluginDeviceSession* out);

// Resolve want_gpu: primary env off only when "0"; else WORLD3D fallback; else on.
bool resolve_plugin_want_gpu(const char* primary_gpu_env);

// Teardown after capture (or early exit). Does not detach_views.
struct PluginTeardownOpts {
  bool clear_pointcloud = false;
  bool clear_tin = false;
  bool abandon_mesh = false;
  bool shutdown_device = true;
  bool destroy_owned_hwnd = true;
};

// Clear overlays / abandon / shutdown / DestroyWindow per |opts|.
void teardown_plugin_device_session(content::Scene3dPresenter* cam,
                                    PluginDeviceSession* session,
                                    const PluginTeardownOpts& opts);

// Destroy owned HWND only (device pointer intentionally leaked / kept).
void destroy_plugin_owned_hwnd(PluginDeviceSession* session);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_SESSION_DEVICE_SESSION_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_DEVICE_SESSION_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_DEVICE_SESSION_H_

#include "app/views/shell/harness/showcase/plugin/common.h"

#include <windows.h>

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace app {

class Browser;

namespace detail {

// Policy for Scene3D plugin showcase HWND + RHI device setup.
struct PluginDeviceSessionOpts {
  // Mode-specific GPU env (e.g. SMT_PLUGIN_MINE_GPU). Falls back to
  // SMT_PLUGIN_WORLD3D_GPU, then default-on when unset.
  const char* gpu_env = nullptr;
  // When true, require a live map_scene_viewport HWND before detach/create.
  bool require_scene_hwnd = true;
  // Drop ContentMapView / FlyCube on the tab before owning a present HWND.
  bool detach_flycube = true;
  // Clear + present once so FlyCube first frame is not an empty backbuffer.
  bool warm_swapchain = false;
  // Stormsurge Null path: skip borrowed HWND (writers + orbit only).
  bool allow_null_without_hwnd = false;
};

// Owned / borrowed RHI present surface for product plugin Scene3D bodies.
struct PluginDeviceSession {
  render::rhi::Device* device = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool want_gpu = false;
};

// Creates present HWND (GPU) or borrows tab child (Null), then initializes
// Device. On failure returns non-zero and leaves |out| partially filled so the
// caller can DestroyWindow / detach_maps.
int prepare_plugin_device_session(Browser& browser,
                                  const PluginDeviceSessionOpts& opts,
                                  PluginDeviceSession* out);

// Resolve want_gpu: primary env off only when "0"; else WORLD3D fallback; else on.
bool resolve_plugin_want_gpu(const char* primary_gpu_env);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_DEVICE_SESSION_H_

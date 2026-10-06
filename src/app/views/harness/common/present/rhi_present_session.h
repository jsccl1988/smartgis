// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_COMMON_PRESENT_RHI_PRESENT_SESSION_H_
#define APP_VIEWS_HARNESS_COMMON_PRESENT_RHI_PRESENT_SESSION_H_

#include <cstdint>
#include <windows.h>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace ui::views {
class DrawHost;
}  // namespace ui::views

namespace app {

class Browser;

namespace detail {

// How GPU-on is resolved from environment variables.
enum class GpuEnvPolicy {
  // Atmosphere: on only when primary env is exactly "1"; else default off.
  kDefaultOffRequireOne,
  // Plugin: off only when env is exactly "0"; else fallback env; else on.
  kDefaultOnUnlessZero,
};

using ShowcasePresentHwndFactory = HWND (*)(uint32_t width_px,
                                            uint32_t height_px);
using ShowcaseMarkFn = void (*)(const char* step);

// Mark strings written on failure / success steps (scene-local wording).
struct RhiPresentSessionMarks {
  const char* scene_hwnd_missing = "scene-hwnd-missing";
  const char* detached = nullptr;  // optional; atmosphere sets "detached"
  const char* present_hwnd_fail = "present-hwnd-fail";
  const char* present_hwnd_ok = "present-hwnd-ok";
  const char* hwnd_missing = "hwnd-missing";
  const char* null_without_hwnd = "bmp-skip-null";
  const char* device_missing = "device-missing";
  const char* device_init_fail = "device-init-fail";
  const char* device_init_gpu = "device-init-gpu";
  const char* device_init_null = "device-init-null";
};

// Shared HWND + RHI Device setup for Scene3D showcase (atmosphere / plugin).
// Does not know scene mode enums or product seeds.
struct RhiPresentSessionOpts {
  const char* gpu_env = nullptr;
  GpuEnvPolicy gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  // Used when |gpu_policy| is kDefaultOnUnlessZero and primary env unset.
  const char* gpu_env_fallback = "plugin-world3d-gpu";

  bool require_scene_hwnd = true;
  // When require_scene_hwnd and viewport lacks HWND, try realize_native once.
  bool realize_scene_hwnd = false;
  bool detach_flycube = true;
  DWORD detach_pump_ms = 0;
  bool warm_swapchain = false;
  // Stormsurge Null path: skip borrowed HWND (writers + orbit only).
  bool allow_null_without_hwnd = false;
  // Product 3D: present into Browser DrawHost Role::kScene3d (tab 1),
  // reusing the pane's FlyCube Device / DXGI popup. Do not CreateWindow a
  // sticky "Plugin Showcase" / "Atmosphere Showcase" HWND.
  bool borrow_shell_scene3d = false;

  uint32_t present_w = 640;
  uint32_t present_h = 480;
  ShowcasePresentHwndFactory create_hwnd = nullptr;

  ShowcaseMarkFn mark = nullptr;
  RhiPresentSessionMarks marks;
};

// Owned / borrowed RHI present surface filled by prepare_rhi_present_session.
struct RhiPresentSession {
  render::rhi::Device* device = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool want_gpu = false;
  // When true, device/HWND belong to DrawHost — never shutdown/Destroy.
  bool borrowed_shell = false;
};

// FlyCube DXGI popup over the 3D pane, else the embed native_view().
HWND shell_scene3d_capture_hwnd(ui::views::DrawHost* scene);

// Kick the shell Display mailbox / present timer; wait until presented
// advances (do not call Device methods on the UI thread).
bool present_shell_scene3d_frame(ui::views::DrawHost* scene,
                                 DWORD wait_ms = 2000);

bool resolve_rhi_want_gpu(const RhiPresentSessionOpts& opts);

// Creates present HWND (GPU) or borrows tab child (Null), then initializes
// Device. On failure returns non-zero and leaves |out| partially filled.
// FlyCube: initialize failure calls shutdown but never operator delete.
int prepare_rhi_present_session(Browser& browser,
                                const RhiPresentSessionOpts& opts,
                                RhiPresentSession* out);

// Overlay / Device / HWND / map detach. Live GPU success paths must keep
// shutdown_device=false — FlyCube operator delete / DX12 shutdown after a live
// present can heap-corrupt ExitProcess.
struct RhiPresentTeardownOpts {
  bool shutdown_device = false;
  bool destroy_hwnd = true;
  bool detach_maps = false;
  bool clear_pointcloud = false;
  bool clear_tin = false;
  bool abandon_mesh = false;
};

// Destroys owned_present_hwnd and clears present_hwnd when they alias.
void destroy_rhi_owned_present_hwnd(RhiPresentSession* session);

// Scene-agnostic teardown. Overlay flags no-op when |cam| is null.
// |browser| is required only when opts.detach_maps is true.
void teardown_rhi_present_session(Browser* browser,
                                  content::Scene3dPresenter* cam,
                                  RhiPresentSession* session,
                                  const RhiPresentTeardownOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_COMMON_PRESENT_RHI_PRESENT_SESSION_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_PRESENT_SESSION_H_
#define IL_RUNTIME_BACKEND_VIEW_PRESENT_SESSION_H_

#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/view/present/env.h"

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

namespace plugin {
class HarnessShell;
}

namespace app {
namespace detail {

using PresentHwndFactory = HWND (*)(uint32_t width_px,
                                            uint32_t height_px);

// Top-level no-activate present window (harness / RHI), not a tab child HWND.
struct PresentHwndOpts {
  const wchar_t* class_name = nullptr;
  const wchar_t* window_title = nullptr;
  uint32_t width_px = 640;
  uint32_t height_px = 480;
};

// Registers (idempotent) |class_name| and creates a visible overlapped HWND.
HWND create_present_hwnd(const PresentHwndOpts& opts);

// DestroyWindow + null. No-op when |hwnd| is null or already null.
void destroy_present_hwnd(HWND* hwnd);

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
  const char* realize_native = nullptr;
  const char* scene_attach = nullptr;
  const char* borrow_ok = "shell-scene3d-borrow";
};

// Shared HWND + RHI Device setup for Scene3D present (atmosphere / plugin).
// Two surfaces: borrow the shell DrawHost (device and HWND stay owned by the
// pane) or create an owned present HWND + Device. Does not know scene mode
// enums or product seeds.
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
  // sticky product present HWND (avoid when borrowing shell Scene3D).
  bool borrow_shell_scene3d = false;
  // select_map_tab(1) AVs on GDI/lazy ContentMapView; atmosphere and some
  // plugin paths borrow without switching tabs.
  bool select_scene_tab = true;

  uint32_t present_w = 640;
  uint32_t present_h = 480;
  // When class_name + window_title are set, present creates the HWND via
  // create_present_hwnd. Factory overrides when non-null.
  PresentHwndOpts hwnd;
  PresentHwndFactory create_hwnd = nullptr;

  StepMarkFn mark = nullptr;
  RhiPresentSessionMarks marks;
};

inline GpuEnvOpts gpu_env_from_session(const RhiPresentSessionOpts& opts) {
  GpuEnvOpts gpu;
  gpu.gpu_env = opts.gpu_env;
  gpu.gpu_policy = opts.gpu_policy;
  gpu.gpu_env_fallback = opts.gpu_env_fallback;
  return gpu;
}

inline bool resolve_rhi_want_gpu(const RhiPresentSessionOpts& opts) {
  return resolve_rhi_want_gpu(gpu_env_from_session(opts));
}

// Owned / borrowed RHI present surface filled by prepare_rhi_present_session.
struct RhiPresentSession {
  render::rhi::Device* device = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool want_gpu = false;
  // When true, device/HWND belong to DrawHost — never shutdown/Destroy.
  bool borrowed_shell = false;
};

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

// Creates present HWND (GPU) or borrows the shell pane, then initializes
// Device. On failure returns non-zero and leaves |out| partially filled.
// FlyCube: initialize failure calls shutdown but never operator delete.
int prepare_rhi_present_session(plugin::HarnessShell& browser,
                                const RhiPresentSessionOpts& opts,
                                RhiPresentSession* out);

// Destroys owned_present_hwnd and clears present_hwnd when they alias.
void destroy_rhi_owned_present_hwnd(RhiPresentSession* session);

// Scene-agnostic teardown. Overlay flags no-op when |cam| is null.
// |browser| is required only when opts.detach_maps is true.
void teardown_rhi_present_session(plugin::HarnessShell* browser,
                                  content::Scene3dPresenter* cam,
                                  RhiPresentSession* session,
                                  const RhiPresentTeardownOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_PRESENT_SESSION_H_

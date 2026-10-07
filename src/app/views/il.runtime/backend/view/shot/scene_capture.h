// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_SHOT_SCENE_CAPTURE_H_
#define IL_RUNTIME_BACKEND_VIEW_SHOT_SCENE_CAPTURE_H_

#include "app/views/il.runtime/backend/view/pixel/bmp.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"

#include <cstdint>
#include <windows.h>

namespace content {
class Scene3dPresenter;
}  // namespace content

namespace render::rhi {
class Device;
}  // namespace render::rhi

namespace app {
namespace detail {

// Optional post-pass after a lit BMP is accepted (e.g. atmosphere labels).
using Scene3dCaptureAfterOkFn = bool (*)(const wchar_t* bmp_path, int w, int h,
                                         void* user);

// Optional GPU readback (stereo GL/D3D). Fills tightly packed bottom-up BGR24
// (|out| = w*h*3). Prefer over HWND BitBlt when the swapchain has no GDI.
using Scene3dGpuReadbackFn = bool (*)(void* user, unsigned char* out, int w,
                                      int h);

// Scene3D HWND → BMP capture. Composes one present, a client read, and the
// pixel gate. Does not choose the software hypsometric fallback.
struct Scene3dHwndCaptureOpts {
  const wchar_t* bmp_leaf = nullptr;
  // Full path wins over bmp_leaf (IL export_bmp already resolved the leaf).
  const wchar_t* bmp_path = nullptr;
  uint32_t present_w = kCaptureW;
  uint32_t present_h = kCaptureH;
  int pre_capture_pump_ms = 80;
  // Globe flythrough: Sleep instead of pumping the shell message.
  bool sleep_instead_of_pump = false;
  // Plugin Null path skips capture; atmosphere still attempts a best-effort BMP.
  bool skip_when_null_gpu = true;
  bool use_grid_lit_policy = false;
  bool retry_dark_frame = false;
  bool require_color_diversity = true;
  bool allow_32bpp = false;
  double mean_min = 0.0;
  double mean_max = 0.0;
  int dst_w = 0;
  int dst_h = 0;
  const char* engine_sidecar = nullptr;
  // Shell DrawHost FlyCube Device is Display-thread only. Do not call
  // present_gpu / Scene3dPresenter::paint from the UI thread.
  bool skip_ui_thread_present = false;
  // Borrowed-shell scenic GDI: export via scenic::Engine memory DIB.
  bool prefer_scenic_export = false;

  StepMarkFn mark = nullptr;
  const char* mark_skip_null = "bmp-skip-null";
  const char* mark_path_fail = "bmp-path-fail";
  const char* mark_skip = "bmp-skip";
  const char* mark_ok = "bmp-ok";
  const char* mark_black = "bmp-black";
  const char* log_prefix = "scene3d-capture";

  Scene3dCaptureAfterOkFn after_ok = nullptr;
  void* after_ok_user = nullptr;
  Scene3dGpuReadbackFn gpu_readback = nullptr;
  void* gpu_readback_user = nullptr;
};

// Present once + HWND BMP + color-diversity gate. When !want_gpu returns true
// after optional skip mark (Null path does not require a visual BMP).
bool capture_scene3d_hwnd_bmp(content::Scene3dPresenter* cam,
                              render::rhi::Device* device,
                              HWND capture_hwnd,
                              bool want_gpu,
                              const Scene3dHwndCaptureOpts& opts);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_SHOT_SCENE_CAPTURE_H_

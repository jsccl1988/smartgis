// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_CAPTURE_SCENE3D_CAPTURE_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_CAPTURE_SCENE3D_CAPTURE_H_

#include "app/views/shell/harness/common/capture/bmp.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"

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

// Scene3D HWND → BMP capture shared by atmosphere / plugin showcases.
// Does not cover map2d export_bmp paths.
struct Scene3dHwndCaptureOpts {
  const wchar_t* bmp_leaf = nullptr;
  uint32_t present_w = 640;
  uint32_t present_h = 480;
  int pre_capture_pump_ms = 80;
  // Globe flythrough: Sleep instead of pumping the shell queue.
  bool sleep_instead_of_pump = false;
  // Plugin Null path skips capture; atmosphere still attempts a best-effort BMP.
  bool skip_when_null_gpu = true;
  bool use_grid_lit_policy = false;
  bool retry_dark_frame = false;
  bool require_color_diversity = true;

  ShowcaseMarkFn mark = nullptr;
  const char* mark_skip_null = "bmp-skip-null";
  const char* mark_path_fail = "bmp-path-fail";
  const char* mark_skip = "bmp-skip";
  const char* mark_ok = "bmp-ok";
  const char* mark_black = "bmp-black";
  const char* log_prefix = "scene3d-capture";

  Scene3dCaptureAfterOkFn after_ok = nullptr;
  void* after_ok_user = nullptr;
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

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_CAPTURE_SCENE3D_CAPTURE_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/capture/capture.h"

#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "plugin/runtime/host/capability/scenario_shell.h"

namespace plugin {
namespace detail {

bool capture_plugin_hwnd_bmp(content::Scene3dPresenter* cam,
                             PluginDeviceSession* session,
                             const PluginCaptureOpts& opts) {
  if (!cam || !session) {
    return false;
  }
  // Real GPU 3D from the borrowed shell Scene3D HWND (FlyCube / RHI). Do not
  // software-project meshes with GDI — that is not the App 3D present path.
  Scene3dHwndCaptureOpts core;
  core.bmp_leaf = opts.bmp_leaf;
  core.present_w = kPluginPresentW;
  core.present_h = kPluginPresentH;
  core.pre_capture_pump_ms = opts.pre_capture_pump_ms;
  core.use_grid_lit_policy = opts.use_grid_lit_policy;
  core.retry_dark_frame = opts.retry_dark_frame;
  core.require_color_diversity = true;
  core.skip_ui_thread_present = session->borrowed_shell;
  core.skip_when_null_gpu = false;
  core.mark = plugin_mark;
  core.log_prefix = "plugin-showcase";

  const bool want_gpu = session->want_gpu || session->borrowed_shell;
  if (capture_scene3d_hwnd_bmp(cam, session->device, session->present_hwnd,
                               want_gpu, core)) {
    return true;
  }
  plugin_mark("bmp-hwnd-fail");
  return false;
}

}  // namespace detail
}  // namespace plugin

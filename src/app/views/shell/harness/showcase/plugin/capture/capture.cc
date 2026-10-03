// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/capture/capture.h"

#include "app/views/shell/harness/common/capture/scene3d_capture.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"

namespace app {
namespace detail {
namespace {

void plugin_mark(const char* step) {
  plugin_showcase_mark(step);
}

}  // namespace

bool capture_plugin_hwnd_bmp(content::Scene3dPresenter* cam,
                             PluginDeviceSession* session,
                             const PluginCaptureOpts& opts) {
  if (!cam || !session) {
    return false;
  }
  Scene3dHwndCaptureOpts core;
  core.bmp_leaf = opts.bmp_leaf;
  core.present_w = kPluginShowcasePresentW;
  core.present_h = kPluginShowcasePresentH;
  core.pre_capture_pump_ms = opts.pre_capture_pump_ms;
  core.use_grid_lit_policy = opts.use_grid_lit_policy;
  core.retry_dark_frame = opts.retry_dark_frame;
  core.require_color_diversity = true;
  core.mark = plugin_mark;
  core.log_prefix = "plugin-showcase";

  return capture_scene3d_hwnd_bmp(cam, session->device, session->present_hwnd,
                                  session->want_gpu, core);
}

}  // namespace detail
}  // namespace app

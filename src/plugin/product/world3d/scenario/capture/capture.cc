// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/capture/capture.h"

#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/shell.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/browser/present/scene3d/session/scene3d_stereo_session.h"

namespace plugin {
namespace detail {
namespace {

struct StereoReadbackUser {
  content::Scene3dStereoSession* stereo = nullptr;
  content::OrbitFrame* orbit = nullptr;
  HWND hwnd = nullptr;
};

bool stereo_gpu_readback(void* user, unsigned char* out, int w, int h) {
  auto* ctx = static_cast<StereoReadbackUser*>(user);
  if (!ctx || !ctx->stereo || !out || w < 8 || h < 8) {
    return false;
  }
  // Borrowed shell HWND already has stereo from tab attach. A second
  // try_attach creates another D3D/GL device on the same HWND and AVs.
  if (!ctx->stereo->is_live()) {
    return false;
  }
  const float yaw = ctx->orbit ? ctx->orbit->yaw() : 0.f;
  const float pitch = ctx->orbit ? ctx->orbit->pitch() : 0.4f;
  const float distance = ctx->orbit ? ctx->orbit->distance() : 3.2f;
  return ctx->stereo->capture_bgr24(out, w, h, yaw, pitch, distance);
}

}  // namespace

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
  core.prefer_scenic_export = content::prefer_scene3d_scenic();
  core.skip_when_null_gpu = false;
  core.mark = plugin_mark;
  core.log_prefix = "plugin-showcase";

  StereoReadbackUser stereo_user;
  if (content::prefer_scene3d_stereo_gl()) {
    if (HarnessShell* shell = plugin_scenario_shell()) {
      stereo_user.stereo = shell->scene3d_stereo();
      stereo_user.orbit = shell->orbit_frame();
      stereo_user.hwnd = session->present_hwnd;
      if (stereo_user.stereo) {
        core.gpu_readback = stereo_gpu_readback;
        core.gpu_readback_user = &stereo_user;
      }
    }
  }

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

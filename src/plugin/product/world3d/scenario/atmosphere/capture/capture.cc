// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/capture/capture.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/browser/present/scene3d/session/scene3d_stereo_session.h"
#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"
#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/shell.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdio>

namespace plugin {
namespace detail {
namespace {

struct StereoReadbackUser {
  content::Scene3dStereoSession* stereo = nullptr;
  content::OrbitFrame* orbit = nullptr;
};

bool stereo_gpu_readback(void* user, unsigned char* out, int w, int h) {
  auto* ctx = static_cast<StereoReadbackUser*>(user);
  if (!ctx || !ctx->stereo || !out || w < 8 || h < 8) {
    return false;
  }
  if (!ctx->stereo->is_live()) {
    return false;
  }
  const float yaw = ctx->orbit ? ctx->orbit->yaw() : 0.f;
  const float pitch = ctx->orbit ? ctx->orbit->pitch() : 0.4f;
  const float distance = ctx->orbit ? ctx->orbit->distance() : 3.2f;
  return ctx->stereo->capture_bgr24(out, w, h, yaw, pitch, distance);
}

}  // namespace

bool capture_atmosphere_named_bmp(content::Scene3dPresenter* cam,
                                  render::rhi::Device* device,
                                  HWND present_hwnd,
                                  HWND owned_present_hwnd,
                                  const wchar_t* bmp_leaf,
                                  bool globe_flythrough,
                                  ui::views::DrawHost* scene) {
  HWND hwnd = owned_present_hwnd ? owned_present_hwnd : present_hwnd;
  if (scene) {
    // DXGI FlyCube popup only — never BitBlt the Map-tab native_view.
    if (HWND live = shell_scene3d_capture_hwnd(scene)) {
      hwnd = live;
    }
  }
  if (!cam || !device || !bmp_leaf || !hwnd || !IsWindow(hwnd)) {
    return false;
  }

  uint32_t cw = kAtmosphereShowcaseW;
  uint32_t ch = kAtmosphereShowcaseH;
  atmosphere_hwnd_present_size(hwnd, &cw, &ch);

  const bool borrowed = scene != nullptr;
  if (borrowed) {
    scene->set_gpu_present_visible(true);
    if (HWND live = shell_scene3d_capture_hwnd(scene)) {
      hwnd = live;
      ShowWindow(live, SW_SHOWNOACTIVATE);
      SetWindowPos(live, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }
    // Display-thread present only — do not call present_gpu on UI thread.
    (void)present_shell_scene3d_frame(scene, 900);
    if (HWND live = shell_scene3d_capture_hwnd(scene)) {
      hwnd = live;
    }
  }

  wchar_t path[MAX_PATH] = {};
  HarnessShell* shell = atmosphere_scenario_shell();
  if (!shell || !shell->capture_path(path, MAX_PATH, bmp_leaf)) {
    return false;
  }

  Scene3dHwndCaptureOpts core;
  core.bmp_path = path;
  core.present_w = cw;
  core.present_h = ch;
  core.dst_w = static_cast<int>(kAtmosphereShowcaseW);
  core.dst_h = static_cast<int>(kAtmosphereShowcaseH);
  core.pre_capture_pump_ms = globe_flythrough ? 0 : 120;
  core.sleep_instead_of_pump = globe_flythrough;
  core.retry_dark_frame = true;
  core.require_color_diversity = true;
  core.allow_32bpp = true;
  core.skip_when_null_gpu = false;
  core.skip_ui_thread_present = borrowed;
  // Borrowed FlyCube: scenic export paints a soft Local-DEM plane + HUD and
  // never samples the live DXGI swapchain (false-green bmp-ok, score fails).
  // Prefer HWND/DWM blit of the present popup after Display-thread present.
  core.prefer_scenic_export = content::prefer_scene3d_scenic() && !borrowed;
  core.mark = atmosphere_mark;
  core.log_prefix = "atmosphere-showcase";
  core.engine_sidecar = "FlyCube/DX12";

  StereoReadbackUser stereo_user;
  if (content::prefer_scene3d_stereo_gl() && shell) {
    stereo_user.stereo = shell->scene3d_stereo();
    stereo_user.orbit = shell->orbit_frame();
    if (stereo_user.stereo) {
      core.gpu_readback = stereo_gpu_readback;
      core.gpu_readback_user = &stereo_user;
    }
  }

  return capture_scene3d_hwnd_bmp(cam, device, hwnd, /*want_gpu=*/true, core);
}

bool capture_atmosphere_showcase_bmp(AtmosphereShowcaseMode /*mode*/,
                                     const char* mode_name,
                                     content::Scene3dPresenter* cam,
                                     render::rhi::Device* device,
                                     HWND present_hwnd,
                                     HWND owned_present_hwnd,
                                     bool want_gpu,
                                     bool globe_flythrough,
                                     ui::views::DrawHost* scene) {
  if (!cam || !device || !mode_name) {
    return !want_gpu;
  }
  wchar_t file[64] = {};
  swprintf_s(file, L"atmosphere-showcase-%S.bmp", mode_name);
  const bool ok = capture_atmosphere_named_bmp(
      cam, device, present_hwnd, owned_present_hwnd, file, globe_flythrough,
      scene);
  if (ok) {
    // capture_scene3d_hwnd_bmp already marks bmp-ok when signal passes.
    std::fprintf(stderr, "atmosphere-showcase: wrote scaled %ux%u %S\n",
                 kAtmosphereShowcaseW, kAtmosphereShowcaseH, mode_name);
    return true;
  }
  if (want_gpu) {
    atmosphere_mark("bmp-skip");
    std::fprintf(stderr, "atmosphere-showcase: BMP lacks visible signal\n");
  }
  return !want_gpu;
}

}  // namespace detail
}  // namespace plugin

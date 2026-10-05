// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdio>
#include <cstdlib>

namespace app {
namespace detail {

int prepare_atmosphere_device_session(Browser& browser,
                                      AtmosphereDeviceSession* out) {
  if (!out) {
    return 50;
  }
  *out = AtmosphereDeviceSession{};

  RhiPresentSessionOpts opts;
  opts.gpu_env = "atmosphere-showcase-gpu";
  opts.gpu_policy = GpuEnvPolicy::kDefaultOffRequireOne;
  opts.gpu_env_fallback = nullptr;
  opts.require_scene_hwnd = true;
  opts.realize_scene_hwnd = true;
  opts.detach_flycube = false;
  // Do not call prepare_rhi_present_session borrow: that select_map_tab(1)
  // AVs on the GDI/lazy ContentMapView path (mark stuck at rhi-borrow).
  opts.borrow_shell_scene3d = false;
  opts.warm_swapchain = false;
  opts.present_w = kAtmosphereShowcaseW;
  opts.present_h = kAtmosphereShowcaseH;
  opts.create_hwnd = nullptr;

  const bool want_gpu = resolve_rhi_want_gpu(opts);
  out->want_gpu = want_gpu;
  // Only warm the swapchain on the GPU path (Null has no backbuffer).
  opts.warm_swapchain = want_gpu;

  ui::views::DrawHost* scene = browser.scene_draw_host();
  // GPU path owns a dedicated present HWND; avoid realize_native on the tab
  // child (ContentMapView attach AVs under GDI-forced showcase + DLL churn).
  if (!want_gpu) {
    if (scene && !scene->native_view()) {
      scene->realize_native();
    }
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      std::fprintf(stderr, "atmosphere-showcase: 3D viewport HWND missing\n");
      return 50;
    }
  }
  // Bind MapContents even with view_id==0 (kNone attach) so present_gpu can
  // resolve DEM / document hosts — same as init_shell before first 3D tab.
  if (content::Scene3dPresenter* cam = browser.scene3d()) {
    const int view_id = scene ? scene->view_id() : 0;
    cam->bind_contents(browser.map_session(), view_id);
  }
  atmosphere_showcase_mark("scene-hwnd-ok");

  out->linger = atmosphere_showcase_linger(want_gpu);

  atmosphere_showcase_mark("rhi-borrow");
  if (!scene) {
    atmosphere_showcase_mark("hwnd-missing");
    std::fprintf(stderr, "atmosphere-showcase: scene viewport missing\n");
    return 50;
  }
  if (!scene->native_view()) {
    scene->realize_native();
    atmosphere_showcase_mark("realize-native");
  }
  scene->sync_native_bounds();
  if (HWND hwnd = scene->native_view()) {
    if (IsWindow(hwnd)) {
      ShowWindow(hwnd, SW_SHOW);
      SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
  }
  if (scene->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
    atmosphere_showcase_mark("scene-attach");
    scene->attach();
    scene->sync_native_bounds();
  }
  scene->set_gpu_present_visible(true);
  scene->resume_present_timer();
  (void)scene->wait_ready(2500);
  const DWORD wait0 = GetTickCount();
  while (!scene->rhi_device() && (GetTickCount() - wait0) < 4000u) {
    pump_messages(50);
  }
  if (content::Scene3dPresenter* cam = browser.scene3d()) {
    cam->bind_contents(browser.map_session(), scene->view_id());
  }
  out->borrowed_shell = true;
  out->owned_present_hwnd = nullptr;
  out->present_hwnd = scene->present_hwnd();
  if (!out->present_hwnd || !IsWindow(out->present_hwnd)) {
    out->present_hwnd = shell_scene3d_capture_hwnd(scene);
  }
  out->device = static_cast<render::rhi::Device*>(scene->rhi_device());
  out->want_gpu = want_gpu;
  out->owns_device = false;
  out->scene = scene;
  if (!out->present_hwnd) {
    atmosphere_showcase_mark("hwnd-missing");
    std::fprintf(stderr, "atmosphere-showcase: capture HWND missing\n");
    return 50;
  }
  atmosphere_showcase_mark("shell-scene3d-borrow");
  atmosphere_showcase_mark("hwnd-ready");
  atmosphere_showcase_mark("device-created");
  std::fprintf(stderr,
               "atmosphere-showcase: gpu=%d linger=%s present=%p %ux%u\n",
               want_gpu ? 1 : 0,
               out->linger.until_close ? "until-close"
                                       : (out->linger.ms > 0 ? "timed" : "none"),
               static_cast<void*>(out->present_hwnd), kAtmosphereShowcaseW,
               kAtmosphereShowcaseH);
  if (!out->linger.until_close && out->linger.ms > 0) {
    std::fprintf(stderr, "atmosphere-showcase: linger_ms=%lu\n",
                 static_cast<unsigned long>(out->linger.ms));
  }
  atmosphere_showcase_mark(want_gpu ? "flycube-ok" : "null-ok");
  return 0;
}

}  // namespace detail
}  // namespace app

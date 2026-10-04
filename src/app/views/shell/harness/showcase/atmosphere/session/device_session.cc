// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "app/views/shell/harness/showcase/atmosphere/session/host.h"
#include "app/views/shell/harness/showcase/atmosphere/common/progress.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "ui/views/map/map_viewport.h"

#include <cstdio>
#include <cstdlib>

namespace app {
namespace detail {
namespace {

void atmosphere_mark(const char* step) {
  atmosphere_showcase_mark(step);
}

}  // namespace

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
  opts.require_scene_hwnd = false;
  opts.detach_flycube = true;
  opts.warm_swapchain = true;
  opts.present_w = kAtmosphereShowcaseW;
  opts.present_h = kAtmosphereShowcaseH;
  opts.create_hwnd = create_atmosphere_showcase_hwnd;
  opts.mark = atmosphere_mark;
  opts.marks.detached = "detached";
  opts.marks.device_init_fail = "device-missing";
  opts.marks.hwnd_missing = "hwnd-missing";

  const bool want_gpu = resolve_rhi_want_gpu(opts);
  out->want_gpu = want_gpu;
  // Only warm the swapchain on the GPU path (Null has no backbuffer).
  opts.warm_swapchain = want_gpu;

  ui::views::MapViewport* scene = browser.map_scene_viewport();
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

  RhiPresentSession core;
  if (const int rc = prepare_rhi_present_session(browser, opts, &core)) {
    out->device = core.device;
    out->present_hwnd = core.present_hwnd;
    out->owned_present_hwnd = core.owned_present_hwnd;
    out->want_gpu = core.want_gpu;
    out->owns_device = (core.device != nullptr);
    if (rc == 50 && want_gpu && !core.owned_present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: present HWND create failed\n");
    } else if (rc == 50 && !want_gpu) {
      std::fprintf(stderr, "atmosphere-showcase: HWND gone after detach\n");
    } else if (rc == 51) {
      std::fprintf(stderr, "atmosphere-showcase: create_device/init failed\n");
    }
    return rc;
  }

  out->device = core.device;
  out->present_hwnd = core.present_hwnd;
  out->owned_present_hwnd = core.owned_present_hwnd;
  out->want_gpu = core.want_gpu;
  out->owns_device = (core.device != nullptr);
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

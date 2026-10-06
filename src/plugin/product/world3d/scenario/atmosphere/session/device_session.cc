// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "plugin/product/world3d/scenario/atmosphere/common/progress.h"

#include <cstdio>

namespace plugin {
namespace detail {

int prepare_atmosphere_device_session(HarnessShell& browser,
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
  opts.borrow_shell_scene3d = true;
  // select_map_tab(1) AVs on the GDI/lazy ContentMapView path.
  opts.select_scene_tab = false;
  opts.warm_swapchain = false;
  opts.present_w = kAtmosphereShowcaseW;
  opts.present_h = kAtmosphereShowcaseH;
  opts.create_hwnd = nullptr;
  opts.mark = atmosphere_mark;
  opts.marks.realize_native = "realize-native";
  opts.marks.scene_attach = "scene-attach";
  opts.marks.borrow_ok = "shell-scene3d-borrow";

  const bool want_gpu = resolve_rhi_want_gpu(opts);
  out->want_gpu = want_gpu;
  out->linger = atmosphere_showcase_linger(want_gpu);

  atmosphere_mark("scene-hwnd-ok");
  atmosphere_mark("rhi-borrow");

  RhiPresentSession core;
  if (const int rc = prepare_rhi_present_session(browser, opts, &core)) {
    out->device = core.device;
    out->present_hwnd = core.present_hwnd;
    out->owned_present_hwnd = core.owned_present_hwnd;
    out->want_gpu = core.want_gpu;
    out->borrowed_shell = core.borrowed_shell;
    out->owns_device = false;
    out->scene = browser.scene_draw_host();
    if (rc == 50) {
      std::fprintf(stderr, "atmosphere-showcase: capture HWND missing\n");
    }
    return rc;
  }

  out->device = core.device;
  out->present_hwnd = core.present_hwnd;
  out->owned_present_hwnd = core.owned_present_hwnd;
  out->want_gpu = core.want_gpu;
  out->borrowed_shell = core.borrowed_shell;
  out->owns_device = false;
  out->scene = browser.scene_draw_host();

  atmosphere_mark("hwnd-ready");
  atmosphere_mark("device-created");
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
  atmosphere_mark(want_gpu ? "flycube-ok" : "null-ok");
  return 0;
}

}  // namespace detail
}  // namespace plugin

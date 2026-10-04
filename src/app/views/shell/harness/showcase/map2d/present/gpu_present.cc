// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/present/gpu_present.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/showcase/map2d/session/gpu_host.h"
#include "app/views/shell/harness/showcase/map2d/common/progress.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "render/rhi/rhi.h"

#include <chrono>
#include <cstdio>

namespace app {
namespace detail {
namespace {

void log_map2d_phase_sample(const char* tag) {
  const content::Map2dPhaseSample s = content::map2d_last_phase_sample();
  std::fprintf(stderr,
               "map2d-showcase: %s layout_ms=%lld hillshade_ms=%lld "
               "software_paint_ms=%lld paint_ms=%lld bmp_io_ms=%lld "
               "gpu_upload_ms=%lld gpu_present_ms=%lld\n",
               tag, static_cast<long long>(s.layout_ms),
               static_cast<long long>(s.hillshade_ms),
               static_cast<long long>(s.software_paint_ms),
               static_cast<long long>(s.software_paint_ms),
               static_cast<long long>(s.bmp_io_ms),
               static_cast<long long>(s.gpu_upload_ms),
               static_cast<long long>(s.gpu_present_ms));
}

}  // namespace

void run_optional_map2d_gpu_present(Browser& browser,
                                    content::Map2dPresenter* map2d,
                                    int showcase_w,
                                    int showcase_h) {
  if (!map2d || !map2d_want_gpu_present()) {
    return;
  }
  map2d_showcase_mark("gpu-try");
  if (map2d->hosts_scenic_present()) {
    map2d_showcase_mark("gpu-present-enter");
    content::reset_map2d_phase_sample();
    const auto t_cold = std::chrono::steady_clock::now();
    const bool ok_cold =
        map2d->present_gpu(nullptr, showcase_w, showcase_h);
    const long long present_gpu_cold_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t_cold)
            .count();
    std::fprintf(stderr,
                 "map2d-showcase: present_gpu=%d present_gpu_ms=%lld "
                 "present_gpu_cold_ms=%lld scenic=1\n",
                 ok_cold ? 1 : 0, present_gpu_cold_ms, present_gpu_cold_ms);
    log_map2d_phase_sample("phase_cold_present");
    map2d_showcase_mark(ok_cold ? "gpu-present-ok" : "gpu-present-fail");

    content::reset_map2d_phase_sample();
    const auto t_warm = std::chrono::steady_clock::now();
    const bool ok_warm =
        map2d->present_gpu(nullptr, showcase_w, showcase_h);
    const long long present_gpu_warm_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t_warm)
            .count();
    std::fprintf(stderr,
                 "map2d-showcase: present_gpu_warm=%d present_gpu_warm_ms=%lld "
                 "scenic=1\n",
                 ok_warm ? 1 : 0, present_gpu_warm_ms);
    log_map2d_phase_sample("phase_warm_present");
    map2d_showcase_mark(ok_warm ? "gpu-warm-ok" : "gpu-warm-fail");
    return;
  }
  bool owned_device = false;
  render::rhi::Device* device = acquire_map2d_showcase_gpu_device(
      browser, showcase_w, showcase_h, &owned_device);
  if (!device) {
    map2d_showcase_mark("gpu-skip");
    std::fprintf(stderr, "map2d-showcase: present_gpu skipped (no device)\n");
    return;
  }

  map2d_showcase_mark("gpu-present-enter");
  std::fprintf(stderr, "map2d-showcase: gpu device acquired owned=%d\n",
               owned_device ? 1 : 0);
  std::fflush(stderr);
  content::reset_map2d_phase_sample();
  const auto t_cold = std::chrono::steady_clock::now();
  const bool ok_cold = map2d->present_gpu(device, showcase_w, showcase_h);
  const long long present_gpu_cold_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - t_cold)
          .count();
  std::fprintf(stderr,
               "map2d-showcase: present_gpu=%d present_gpu_ms=%lld "
               "present_gpu_cold_ms=%lld\n",
               ok_cold ? 1 : 0, present_gpu_cold_ms, present_gpu_cold_ms);
  log_map2d_phase_sample("phase_cold_present");
  map2d_showcase_mark(ok_cold ? "gpu-present-ok" : "gpu-present-fail");

  content::reset_map2d_phase_sample();
  const auto t_warm = std::chrono::steady_clock::now();
  const bool ok_warm = map2d->present_gpu(device, showcase_w, showcase_h);
  const long long present_gpu_warm_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - t_warm)
          .count();
  std::fprintf(stderr,
               "map2d-showcase: present_gpu_warm=%d present_gpu_warm_ms=%lld\n",
               ok_warm ? 1 : 0, present_gpu_warm_ms);
  log_map2d_phase_sample("phase_warm_present");
  map2d_showcase_mark(ok_warm ? "gpu-warm-ok" : "gpu-warm-fail");

  if (owned_device) {
    device->shutdown();
    // Intentionally leak Device* -- same FlyCube teardown policy as
    // atmosphere showcase / MapViewport.
  }
}

}  // namespace detail
}  // namespace app

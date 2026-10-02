// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/present_run.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/map2d/capture.h"
#include "app/views/shell/harness/showcase/map2d/gpu_host.h"
#include "app/views/shell/harness/showcase/map2d/sample.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <windows.h>

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

void run_optional_gpu_present(Browser& browser,
                              content::Map2dPresenter* map2d,
                              int showcase_w,
                              int showcase_h) {
  if (!map2d_want_gpu_present()) {
    return;
  }
  map2d_showcase_mark("gpu-try");
  bool owned_device = false;
  render::rhi::Device* device = acquire_map2d_showcase_gpu_device(
      browser, showcase_w, showcase_h, &owned_device);
  if (!device) {
    map2d_showcase_mark("gpu-skip");
    std::fprintf(stderr, "map2d-showcase: present_gpu skipped (no device)\n");
    return;
  }

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
    // Intentionally leak Device* — same FlyCube teardown policy as
    // atmosphere showcase / MapViewport.
  }
}

void run_optional_fps_bench(Browser& browser, content::Map2dPresenter* map2d) {
  const char* bench_env = std::getenv("SMT_MAP2D_FPS_BENCH_MS");
  if (!bench_env) {
    return;
  }
  const int bench_ms = std::atoi(bench_env);
  if (bench_ms <= 0 || !map2d) {
    return;
  }
  map2d_showcase_mark("fps-bench");
  ui::views::MapViewport* pane = browser.map_viewport();
  float sum = 0.f;
  float peak = 0.f;
  int samples = 0;
  const uint64_t builds0 = map2d->layout_build_count();
  content::reset_map2d_gpu_present_profile();
  // Warm past dual-speed settle (~200ms) so StaticReuse dominates samples.
  if (pane) {
    pane->invalidate_native();
    pump_views_messages(350);
  }
  const DWORD t0 = GetTickCount();
  while (static_cast<int>(GetTickCount() - t0) < bench_ms) {
    if (pane) {
      pane->invalidate_native();
      // Avoid sync_identity_chrome — it churns shell overlay generation.
      const float fps = pane->hud_fps();
      // Skip first 250ms of samples (settle + first StaticReuse).
      const int elapsed = static_cast<int>(GetTickCount() - t0);
      if (fps > 0.f && elapsed >= 250) {
        sum += fps;
        if (fps > peak) {
          peak = fps;
        }
        ++samples;
      }
    }
    pump_views_messages(16);
  }
  const float mean = samples > 0 ? (sum / static_cast<float>(samples)) : 0.f;
  const uint64_t builds_delta = map2d->layout_build_count() - builds0;
  const content::Map2dGpuPresentProfile prof =
      content::map2d_gpu_present_profile();
  const uint64_t present_n = prof.skip + prof.full;
  const float skip_pct =
      present_n > 0 ? (100.f * static_cast<float>(prof.skip) /
                       static_cast<float>(present_n))
                    : 0.f;
  std::fprintf(stderr,
               "map2d-showcase: fps_bench ms=%d samples=%d mean=%.2f "
               "peak=%.2f layout_builds_delta=%llu total=%llu "
               "gpu_skip=%llu gpu_full=%llu skip_pct=%.1f "
               "act_r/i/s/st=%llu/%llu/%llu/%llu\n",
               bench_ms, samples, mean, peak,
               static_cast<unsigned long long>(builds_delta),
               static_cast<unsigned long long>(map2d->layout_build_count()),
               static_cast<unsigned long long>(prof.skip),
               static_cast<unsigned long long>(prof.full), skip_pct,
               static_cast<unsigned long long>(prof.action_rebuild),
               static_cast<unsigned long long>(prof.action_interactive),
               static_cast<unsigned long long>(prof.action_settle),
               static_cast<unsigned long long>(prof.action_static));
  wchar_t bench_w[MAX_PATH] = {};
  if (exe_capture_path(bench_w, MAX_PATH, L"map2d-fps-bench.txt")) {
    FILE* bf = nullptr;
    if (_wfopen_s(&bf, bench_w, L"w") == 0 && bf) {
      std::fprintf(bf,
                   "mean_fps=%.3f\npeak_fps=%.3f\nsamples=%d\n"
                   "bench_ms=%d\nlayout_builds=%llu\n"
                   "layout_builds_delta=%llu\n"
                   "gpu_skip=%llu\ngpu_full=%llu\nskip_pct=%.1f\n"
                   "action_rebuild=%llu\naction_interactive=%llu\n"
                   "action_settle=%llu\naction_static=%llu\n",
                   mean, peak, samples, bench_ms,
                   static_cast<unsigned long long>(map2d->layout_build_count()),
                   static_cast<unsigned long long>(builds_delta),
                   static_cast<unsigned long long>(prof.skip),
                   static_cast<unsigned long long>(prof.full), skip_pct,
                   static_cast<unsigned long long>(prof.action_rebuild),
                   static_cast<unsigned long long>(prof.action_interactive),
                   static_cast<unsigned long long>(prof.action_settle),
                   static_cast<unsigned long long>(prof.action_static));
      std::fclose(bf);
    }
  }
  map2d_showcase_mark("fps-bench-done");
}

}  // namespace

int run_map2d_present(Browser& browser,
                      const char* mode_name,
                      int showcase_w,
                      int showcase_h) {
  Map2dCapturePaths paths;
  if (const int rc = prepare_map2d_capture_paths(mode_name, &paths)) {
    return rc;
  }

  content::Map2dPresenter* map2d = browser.map2d();
  if (!map2d) {
    std::fprintf(stderr, "map2d-showcase: Map2dPresenter missing\n");
    return 57;
  }
  // frame_china_map2d / orthogrid already invalidated when size/extent changed.
  // Do not invalidate again immediately before timed present — that forces a
  // cold layout+upload into the present_gpu wall clock.
  map2d_showcase_mark("cache-ready");

  // Capture uses software export_bmp — do NOT UpdateWindow here. Sync GDI
  // paint through the HWND has AVd in Map2dSoftwarePainter / ContentMapView
  // under parallel harness (mark stops at bmp-path). Async InvalidateRect is
  // enough so the live HWND may refresh; BMP does not depend on it.
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
    }
  }
  pump_views_messages(50);
  map2d_showcase_mark("overlay-paint");

  // Warm layout+hillshade AFTER HWND pump: a live paint at client size would
  // otherwise rebuild MapFrame at ~2k and clobber the showcase 1280x720 cache.
  if (!map2d->frame_cache().ensure_full(static_cast<uint32_t>(showcase_w),
                                        static_cast<uint32_t>(showcase_h))) {
    std::fprintf(stderr, "map2d-showcase: ensure_full layout failed\n");
    return 57;
  }
  map2d_showcase_mark("layout-warm");

  // Software BMP first — carto gates / review-prep must not depend on optional
  // FlyCube smoke. Prior order (GPU then export) left bmp_missing when
  // present_gpu AVd on a second DXGI chain (ContentMapView HWND).
  if (const int rc =
          export_map2d_showcase_bmp(map2d, paths, showcase_w, showcase_h)) {
    return rc;
  }
  if (const int rc = verify_map2d_showcase_bmp(mode_name, paths)) {
    return rc;
  }

  // Optional FlyCube present smoke after BMP (src/render RHI 2D). Capture
  // already landed above; accept_nonzero_rc_if_bmp covers a late GPU fail.
  // Reuse one Device across cold + warm samples; report both separately.
  run_optional_gpu_present(browser, map2d, showcase_w, showcase_h);

  // Optional FPS bench: keep maps live, request presents, sample HUD FPS.
  // SMT_MAP2D_FPS_BENCH_MS=3000 (default off). Writes map2d-fps-bench.txt.
  run_optional_fps_bench(browser, map2d);
  return 0;
}

}  // namespace detail
}  // namespace app

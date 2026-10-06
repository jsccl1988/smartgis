// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/fps_bench.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/map2d/scenario/progress.h"
#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "ui/views/map/viewport/draw_host.h"

#include "vista/component/map/detail/hillshade_bake.h"

#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include "base/process/switches.h"

namespace plugin {
namespace detail {

void run_optional_map2d_fps_bench(HarnessShell& browser,
                                  content::Map2dPresenter* map2d) {
  const char* bench_env = base::switch_cstr("map2d-fps-bench-ms");
  if (!bench_env) {
    return;
  }
  const int bench_ms = std::atoi(bench_env);
  if (bench_ms <= 0 || !map2d) {
    return;
  }
  map2d_showcase_mark("fps-bench");
  ui::views::DrawHost* pane = browser.draw_host();
  float sum = 0.f;
  float peak = 0.f;
  int samples = 0;
  const uint64_t builds0 = map2d->layout_build_count();
  content::reset_map2d_gpu_present_profile();
  // Warm past dual-speed settle (~200ms) so StaticReuse dominates samples.
  if (pane) {
    pane->invalidate_native();
    browser.pump(350);
  }
  const DWORD t0 = GetTickCount();
  while (static_cast<int>(GetTickCount() - t0) < bench_ms) {
    if (pane) {
      pane->invalidate_native();
      // Avoid sync_identity_frame �?it churns shell overlay generation.
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
    browser.pump(16);
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
  const vista::HillshadeBakeSample bake = vista::hillshade_last_bake_sample();
  std::fprintf(stderr,
               "map2d-showcase: fps_bench ms=%d samples=%d mean=%.2f "
               "peak=%.2f layout_builds_delta=%llu total=%llu "
               "gpu_skip=%llu gpu_full=%llu skip_pct=%.1f "
               "act_r/i/s/st=%llu/%llu/%llu/%llu "
               "bake_mem/disk/load/shade/store_ms=%lld/%lld/%lld/%lld/%lld "
               "bake_hit_mem/disk=%d/%d bake_cuda=%d bake_wh=%dx%d\n",
               bench_ms, samples, mean, peak,
               static_cast<unsigned long long>(builds_delta),
               static_cast<unsigned long long>(map2d->layout_build_count()),
               static_cast<unsigned long long>(prof.skip),
               static_cast<unsigned long long>(prof.full), skip_pct,
               static_cast<unsigned long long>(prof.action_rebuild),
               static_cast<unsigned long long>(prof.action_interactive),
               static_cast<unsigned long long>(prof.action_settle),
               static_cast<unsigned long long>(prof.action_static),
               static_cast<long long>(bake.mem_ms),
               static_cast<long long>(bake.disk_ms),
               static_cast<long long>(bake.load_ms),
               static_cast<long long>(bake.shade_ms),
               static_cast<long long>(bake.store_ms), bake.mem_hit,
               bake.disk_hit, bake.used_cuda, bake.width, bake.height);
  wchar_t bench_w[MAX_PATH] = {};
  if (browser.capture_path(bench_w, MAX_PATH, L"map2d-fps-bench.txt")) {
    FILE* bf = nullptr;
    if (_wfopen_s(&bf, bench_w, L"w") == 0 && bf) {
      std::fprintf(bf,
                   "mean_fps=%.3f\npeak_fps=%.3f\nsamples=%d\n"
                   "bench_ms=%d\nlayout_builds=%llu\n"
                   "layout_builds_delta=%llu\n"
                   "gpu_skip=%llu\ngpu_full=%llu\nskip_pct=%.1f\n"
                   "action_rebuild=%llu\naction_interactive=%llu\n"
                   "action_settle=%llu\naction_static=%llu\n"
                   "bake_mem_ms=%lld\nbake_disk_ms=%lld\n"
                   "bake_load_ms=%lld\nbake_shade_ms=%lld\n"
                   "bake_store_ms=%lld\nbake_mem_hit=%d\n"
                   "bake_disk_hit=%d\nbake_used_cuda=%d\n"
                   "bake_w=%d\nbake_h=%d\nbake_max_edge=%d\n",
                   mean, peak, samples, bench_ms,
                   static_cast<unsigned long long>(map2d->layout_build_count()),
                   static_cast<unsigned long long>(builds_delta),
                   static_cast<unsigned long long>(prof.skip),
                   static_cast<unsigned long long>(prof.full), skip_pct,
                   static_cast<unsigned long long>(prof.action_rebuild),
                   static_cast<unsigned long long>(prof.action_interactive),
                   static_cast<unsigned long long>(prof.action_settle),
                   static_cast<unsigned long long>(prof.action_static),
                   static_cast<long long>(bake.mem_ms),
                   static_cast<long long>(bake.disk_ms),
                   static_cast<long long>(bake.load_ms),
                   static_cast<long long>(bake.shade_ms),
                   static_cast<long long>(bake.store_ms), bake.mem_hit,
                   bake.disk_hit, bake.used_cuda, bake.width, bake.height,
                   bake.max_edge);
      std::fclose(bf);
    }
  }
  map2d_showcase_mark("fps-bench-done");
}

}  // namespace detail
}  // namespace plugin

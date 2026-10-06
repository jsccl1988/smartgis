// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/plugin/present/present_warmup.h"

#include "app/views/browser/browser.h"
#include "app/views/harness/common/io/maps.h"
#include "app/views/harness/common/pump/pump.h"
#include "app/views/harness/showcase/plugin/common/plugin_io.h"
#include "app/views/harness/common/present/rhi_present_session.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "ui/views/map/viewport/draw_host.h"
#include "app/views/harness/showcase/plugin/seed/world3d_seed.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/present/scene3d/scene3d_phase_profile.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace app {
namespace detail {
namespace {

struct PluginWarmupFail {
  content::Scene3dPresenter* cam = nullptr;
  PluginDeviceSession* session = nullptr;
  Browser* browser = nullptr;
  const char* fail_log_prefix = nullptr;
  PluginPresentFailPolicy on_fail;
};

void on_plugin_warmup_fail(int failed_frame, void* user) {
  auto* ctx = static_cast<PluginWarmupFail*>(user);
  if (!ctx) {
    return;
  }
  std::fprintf(stderr, "plugin-showcase: %s present_gpu failed frame %d\n",
               ctx->fail_log_prefix ? ctx->fail_log_prefix : "scene3d",
               failed_frame);
  PluginTeardownOpts teardown;
  teardown.clear_pointcloud = ctx->on_fail.clear_pointcloud;
  teardown.clear_tin = ctx->on_fail.clear_tin;
  teardown.abandon_mesh = ctx->on_fail.abandon_mesh;
  teardown.shutdown_device = ctx->on_fail.shutdown_device;
  teardown.destroy_owned_hwnd = true;
  teardown_plugin_device_session(ctx->cam, ctx->session, teardown);
  plugin_showcase_mark("present-fail");
  if (ctx->browser) {
    finish_scene3d_showcase(*ctx->browser,
                            ctx->session && ctx->session->borrowed_shell);
  }
}

void write_plugin_present_perf_json(const char* leaf,
                                    const char* mode,
                                    const std::vector<double>& frame_ms,
                                    int discard_cold,
                                    int discard_tail,
                                    int want_gpu,
                                    const content::Scene3dPhaseSample& phase) {
  if (!leaf || !leaf[0] || frame_ms.empty()) {
    return;
  }
  const int present_count = static_cast<int>(frame_ms.size());
  int head =
      (discard_cold < 0) ? 0
                         : ((discard_cold >= present_count) ? present_count - 1
                                                            : discard_cold);
  int tail =
      (discard_tail < 0) ? 0
                         : ((discard_tail >= present_count) ? 0 : discard_tail);
  if (head + tail >= present_count) {
    // Keep at least one sample for warm average.
    if (head > 0) {
      --head;
    } else if (tail > 0) {
      --tail;
    }
  }
  double present_ms_all = 0.0;
  for (double ms : frame_ms) {
    present_ms_all += ms;
  }
  double present_ms_warm = 0.0;
  const int warm_begin = head;
  const int warm_end = present_count - tail;
  const int warm_count = warm_end - warm_begin;
  std::vector<double> warm_sorted;
  warm_sorted.reserve(static_cast<size_t>(warm_count > 0 ? warm_count : 0));
  for (int i = warm_begin; i < warm_end; ++i) {
    const double ms = frame_ms[static_cast<size_t>(i)];
    present_ms_warm += ms;
    warm_sorted.push_back(ms);
  }
  const double ms_all =
      present_ms_all / static_cast<double>(present_count);
  const double ms_warm_mean =
      warm_count > 0 ? present_ms_warm / static_cast<double>(warm_count) : 0.0;
  // Median resists a single mid-loop DXGI hitch (~20 ms) that mean cannot.
  double ms_warm = ms_warm_mean;
  if (warm_count > 0) {
    std::sort(warm_sorted.begin(), warm_sorted.end());
    const size_t mid = warm_sorted.size() / 2;
    if ((warm_sorted.size() % 2) != 0) {
      ms_warm = warm_sorted[mid];
    } else {
      ms_warm = 0.5 * (warm_sorted[mid - 1] + warm_sorted[mid]);
    }
  }
  const double ms_cold = frame_ms[0];

  const content::Scene3dColdPhaseSample cold =
      content::scene3d_cold_phase_sample();
  char perf_path[MAX_PATH] = {};
  if (!exe_capture_path_a(perf_path, MAX_PATH, leaf)) {
    return;
  }
  if (FILE* pf = nullptr; fopen_s(&pf, perf_path, "wb") == 0 && pf) {
    std::fprintf(
        pf,
        "{\"backend\":\"plugin.world3d\",\"mode\":\"%s\","
        "\"present_count\":%d,\"discard_cold\":%d,\"discard_tail\":%d,"
        "\"warm_count\":%d,"
        "\"present_ms\":%.3f,\"present_ms_warm\":%.3f,"
        "\"ms_per_present\":%.3f,\"ms_per_present_mean\":%.3f,"
        "\"ms_per_present_all\":%.3f,"
        "\"ms_per_present_cold\":%.3f,\"gpu\":%d,"
        "\"mesh_ms\":%lld,\"sync_ms\":%lld,\"rebuild_ms\":%lld,"
        "\"rebuild_count\":%d,\"ocean_prep_ms\":%lld,"
        "\"record_ms\":%lld,\"present_swap_ms\":%lld,"
        "\"upload_ms\":%lld,\"pso_ms\":%lld,"
        "\"dem_load_ms\":%lld,\"tess_ms\":%lld,\"hypso_ms\":%lld,"
        "\"cold_phase\":{"
        "\"dem_load_ms\":%lld,\"tess_ms\":%lld,\"hypso_ms\":%lld,"
        "\"upload_ms\":%lld,\"pso_ms\":%lld,\"record_ms\":%lld,"
        "\"mesh_ms\":%lld,\"sync_ms\":%lld,\"rebuild_ms\":%lld,"
        "\"present_ms\":%lld,\"load_cache_hit\":%d,\"hypso_cache_hit\":%d"
        "},\"frame_ms\":[",
        mode && mode[0] ? mode : "plugin", present_count, head, tail,
        warm_count, present_ms_all, present_ms_warm, ms_warm, ms_warm_mean,
        ms_all, ms_cold, want_gpu, static_cast<long long>(phase.mesh_ms),
        static_cast<long long>(phase.sync_ms),
        static_cast<long long>(phase.rebuild_ms), phase.rebuild_count,
        static_cast<long long>(phase.ocean_prep_ms),
        static_cast<long long>(phase.record_ms),
        static_cast<long long>(phase.present_ms),
        static_cast<long long>(phase.upload_ms),
        static_cast<long long>(phase.pso_ms),
        static_cast<long long>(cold.dem_load_ms),
        static_cast<long long>(cold.tess_ms),
        static_cast<long long>(cold.hypso_ms),
        static_cast<long long>(cold.dem_load_ms),
        static_cast<long long>(cold.tess_ms),
        static_cast<long long>(cold.hypso_ms),
        static_cast<long long>(cold.upload_ms),
        static_cast<long long>(cold.pso_ms),
        static_cast<long long>(cold.record_ms),
        static_cast<long long>(cold.mesh_ms),
        static_cast<long long>(cold.sync_ms),
        static_cast<long long>(cold.rebuild_ms),
        static_cast<long long>(cold.present_ms), cold.load_cache_hit,
        cold.hypso_cache_hit);
    for (int i = 0; i < present_count; ++i) {
      std::fprintf(pf, "%s%.3f", i ? "," : "",
                   frame_ms[static_cast<size_t>(i)]);
    }
    std::fprintf(pf, "]}\n");
    std::fclose(pf);
  }
  std::fprintf(stderr,
               "plugin-showcase: present_count=%d discard_cold=%d "
               "discard_tail=%d ms/p_warm(med)=%.2f ms/p_mean=%.2f "
               "ms/p_all=%.2f ms/p_cold=%.2f "
               "cold(dem=%lld tess=%lld hypso=%lld upload=%lld pso=%lld)\n",
               present_count, head, tail, ms_warm, ms_warm_mean, ms_all,
               ms_cold,
               static_cast<long long>(cold.dem_load_ms),
               static_cast<long long>(cold.tess_ms),
               static_cast<long long>(cold.hypso_ms),
               static_cast<long long>(cold.upload_ms),
               static_cast<long long>(cold.pso_ms));
}

}  // namespace

int present_plugin_warmup_frames(content::Scene3dPresenter* cam,
                                 PluginDeviceSession* session,
                                 Browser& browser,
                                 const char* fail_log_prefix,
                                 const PluginPresentFailPolicy& on_fail,
                                 int frame_count) {
  return present_plugin_warmup_frames(cam, session, browser, fail_log_prefix,
                                      on_fail, frame_count, nullptr, nullptr);
}

int present_plugin_warmup_frames(content::Scene3dPresenter* cam,
                                 PluginDeviceSession* session,
                                 Browser& browser,
                                 const char* fail_log_prefix,
                                 const PluginPresentFailPolicy& on_fail,
                                 int frame_count,
                                 const char* perf_json_leaf,
                                 const char* mode) {
  if (!cam || !session) {
    return 52;
  }
  // Scenic GDI present does not need a FlyCube/RHI Device*.
  if (!session->device && !cam->hosts_scenic_present() &&
      !session->borrowed_shell && !content::prefer_scene3d_gdi()) {
    return 52;
  }
  PluginWarmupFail fail_ctx;
  fail_ctx.cam = cam;
  fail_ctx.session = session;
  fail_ctx.browser = &browser;
  fail_ctx.fail_log_prefix = fail_log_prefix;
  fail_ctx.on_fail = on_fail;

  const bool bare = world3d_perf_bare_enabled();
  // Perf-bare: no pump Sleep in the timed loop; discard first cold upload and
  // last DXGI flip-queue hitch (windowed DWM, even with ALLOW_TEARING).
  const int pump_ms = bare ? 0 : 50;
  const int discard_cold = bare ? 1 : 0;
  const int discard_tail = bare ? 1 : 0;

  content::reset_scene3d_phase_sample();

  LARGE_INTEGER qpf = {};
  QueryPerformanceFrequency(&qpf);
  std::vector<double> frame_ms;
  frame_ms.reserve(static_cast<size_t>(frame_count > 0 ? frame_count : 0));
  content::Scene3dPhaseSample warm_phase{};
  bool have_warm_phase = false;

  for (int i = 0; i < frame_count; ++i) {
    LARGE_INTEGER t0 = {};
    LARGE_INTEGER t1 = {};
    if (qpf.QuadPart > 0) {
      QueryPerformanceCounter(&t0);
    }
    bool ok = true;
    if (session->borrowed_shell) {
      ok = present_shell_scene3d_frame(browser.scene_draw_host(), 800);
    } else {
      ok = cam->present_gpu(session->device, kPluginShowcasePresentW,
                            kPluginShowcasePresentH);
    }
    if (!ok) {
      // Scenic stub / first-frame miss: software BMP still paints local DEM.
      plugin_showcase_mark("present-soft");
      if (i + 1 >= frame_count) {
        break;
      }
      continue;
    }
    if (i == 0) {
      content::scene3d_capture_cold_phase();
    }
    if (qpf.QuadPart > 0) {
      QueryPerformanceCounter(&t1);
      frame_ms.push_back(1000.0 *
                         static_cast<double>(t1.QuadPart - t0.QuadPart) /
                         static_cast<double>(qpf.QuadPart));
    } else {
      frame_ms.push_back(0.0);
    }
    // Keep phases from the last frame that still counts as warm.
    const int idx = static_cast<int>(frame_ms.size()) - 1;
    if (idx >= discard_cold && idx < frame_count - discard_tail) {
      warm_phase = content::scene3d_last_phase_sample();
      have_warm_phase = true;
    }
    if (pump_ms > 0) {
      pump_messages(static_cast<DWORD>(pump_ms));
    }
  }
  plugin_showcase_mark("present-ok");

  if (perf_json_leaf && perf_json_leaf[0] && !frame_ms.empty()) {
    if (!have_warm_phase) {
      warm_phase = content::scene3d_last_phase_sample();
    }
    write_plugin_present_perf_json(perf_json_leaf, mode, frame_ms, discard_cold,
                                   discard_tail, session->want_gpu ? 1 : 0,
                                   warm_phase);
  }
  return 0;
}

}  // namespace detail
}  // namespace app

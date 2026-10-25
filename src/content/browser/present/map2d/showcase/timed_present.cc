// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/showcase/timed_present.h"

#include "base/process/switches.h"
#include "content/browser/glang.runtime/host/bind/pane.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "content/browser/debug/capture/capture_host.h"
#include "content/browser/present/scene3d/profile/scene3d_phase_profile.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace content {
namespace detail {
namespace {

int parse_present_count_switch(const char* key) {
  const char* raw = base::switch_cstr(key);
  if (!raw || !raw[0]) {
    return 0;
  }
  const int v = std::atoi(raw);
  if (v < 1 || v > 600) {
    return 0;
  }
  return v;
}

void write_scene3d_present_perf_json(const char* leaf,
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
  int head = (discard_cold < 0)
                 ? 0
                 : ((discard_cold >= present_count) ? present_count - 1
                                                    : discard_cold);
  int tail = (discard_tail < 0)
                 ? 0
                 : ((discard_tail >= present_count) ? 0 : discard_tail);
  if (head + tail >= present_count) {
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
        "{\"backend\":\"scene3d\",\"mode\":\"%s\","
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
        mode && mode[0] ? mode : "scene3d", present_count, head, tail,
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
               "scene3d-timed: present_count=%d discard_cold=%d "
               "discard_tail=%d ms/p_warm(med)=%.2f ms/p_mean=%.2f "
               "ms/p_all=%.2f ms/p_cold=%.2f "
               "ocean_prep=%lld record=%lld swap=%lld rebuild_n=%d\n",
               present_count, head, tail, ms_warm, ms_warm_mean, ms_all,
               ms_cold, static_cast<long long>(phase.ocean_prep_ms),
               static_cast<long long>(phase.record_ms),
               static_cast<long long>(phase.present_ms), phase.rebuild_count);
}

}  // namespace

int resolve_timed_present_count() {
  if (const int n =
          parse_present_count_switch("atmosphere-showcase-present-count")) {
    return n;
  }
  if (const int n =
          parse_present_count_switch("plugin-world3d-present-count")) {
    return n;
  }
  return parse_present_count_switch("scene3d-present-count");
}

int run_timed_scene3d_presents(content::Pane* scene,
                               const TimedPresentOpts& opts) {
  if (!scene || opts.frame_count <= 0) {
    return 0;
  }
  const int frame_count = opts.frame_count;
  // Equal-profile: never PeekMessage/map2d-paint into a long timed window.
  const int pump_ms = (frame_count > 3) ? 0 : opts.pump_ms;
  const int discard_cold = opts.discard_cold;
  const int discard_tail = opts.discard_tail;
  const DWORD wait_ms =
      opts.present_wait_ms > 0 ? opts.present_wait_ms : 800u;

  content::reset_scene3d_phase_sample();

  LARGE_INTEGER qpf = {};
  QueryPerformanceFrequency(&qpf);
  std::vector<double> frame_ms;
  frame_ms.reserve(static_cast<size_t>(frame_count));
  content::Scene3dPhaseSample warm_phase{};
  bool have_warm_phase = false;

  const bool use_gpu =
      opts.cam != nullptr && opts.device != nullptr && opts.width_px > 0 &&
      opts.height_px > 0;
  // UI-thread present_gpu races the Display Flip mailbox and can hitch for
  // hundreds of ms — pause the shell present timer for the timed window.
  const bool paused_display =
      use_gpu && scene && static_cast<bool>(scene->pause_present);
  if (paused_display) {
    scene->pause_present();
  }
  if (use_gpu) {
    std::fprintf(stderr,
                 "scene3d-timed: present_gpu locked %ux%u frames=%d pump=%d "
                 "display_paused=%d\n",
                 opts.width_px, opts.height_px, frame_count, pump_ms,
                 paused_display ? 1 : 0);
  } else {
    std::fprintf(stderr,
                 "scene3d-timed: shell present frames=%d pump=%d wait=%u\n",
                 frame_count, pump_ms, static_cast<unsigned>(wait_ms));
  }

  for (int i = 0; i < frame_count; ++i) {
    LARGE_INTEGER t0 = {};
    LARGE_INTEGER t1 = {};
    if (qpf.QuadPart > 0) {
      QueryPerformanceCounter(&t0);
    }
    bool ok = false;
    if (use_gpu) {
      ok = present_scene3d_gpu(opts.cam, opts.device, opts.width_px,
                               opts.height_px);
    } else {
      ok = present_shell_scene3d_frame(scene, wait_ms);
    }
    if (!ok) {
      std::fprintf(stderr,
                   "scene3d-timed: present failed frame %d (soft continue)\n",
                   i);
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
    const int idx = static_cast<int>(frame_ms.size()) - 1;
    if (idx >= discard_cold && idx < frame_count - discard_tail) {
      warm_phase = content::scene3d_last_phase_sample();
      have_warm_phase = true;
    }
    if (pump_ms > 0) {
      pump_messages(static_cast<DWORD>(pump_ms));
    }
  }

  if (paused_display && scene && static_cast<bool>(scene->resume_present_timer)) {
    scene->resume_present_timer();
  }
  if (frame_ms.empty()) {
    return 52;
  }
  if (!have_warm_phase) {
    warm_phase = content::scene3d_last_phase_sample();
  }
  if (opts.perf_json_leaf && opts.perf_json_leaf[0]) {
    write_scene3d_present_perf_json(opts.perf_json_leaf, opts.mode, frame_ms,
                                    discard_cold, discard_tail, opts.want_gpu,
                                    warm_phase);
  }
  return 0;
}

}  // namespace detail
}  // namespace content

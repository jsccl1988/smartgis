// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/atmosphere/present/present_run.h"

#include "app/views/browser/browser.h"
#include "app/views/harness/common/present/present_gpu_warmup.h"
#include "app/views/harness/showcase/atmosphere/capture/capture.h"
#include "app/views/harness/showcase/atmosphere/present/globe_present.h"
#include "app/views/harness/showcase/atmosphere/present/present_linger.h"
#include "app/views/harness/showcase/atmosphere/common/progress.h"
#include "app/views/harness/showcase/atmosphere/session/session_finish.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_phase_profile.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"

#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {
namespace {

struct AtmosphereWarmupFail {
  Browser* browser = nullptr;
  AtmosphereDeviceSession* session = nullptr;
  content::Scene3dPresenter* cam = nullptr;
  bool timed = false;
  bool want_gpu = false;
  bool globe_flythrough = false;
  AtmosphereShowcaseMode mode = AtmosphereShowcaseMode::kNone;
  const char* name = nullptr;
  HWND present_hwnd = nullptr;
  HWND owned_present_hwnd = nullptr;
  bool* early_bmp_ok = nullptr;
};

void on_atmosphere_warmup_fail(int /*failed_frame*/, void* user) {
  auto* f = static_cast<AtmosphereWarmupFail*>(user);
  if (!f || !f->browser || !f->session || !f->cam) {
    return;
  }
  if (f->timed && f->want_gpu && f->early_bmp_ok && !*f->early_bmp_ok) {
    *f->early_bmp_ok = capture_atmosphere_showcase_bmp(
        f->mode, f->name, f->cam, f->session->device, f->present_hwnd,
        f->owned_present_hwnd, f->want_gpu, f->globe_flythrough,
        f->session->scene);
  }
  f->cam->abandon_mesh();
  finish_atmosphere_device_session(*f->browser, f->session,
                                   /*shutdown_device=*/true);
}

}  // namespace

int run_atmosphere_present(Browser& browser,
                           AtmosphereShowcaseMode mode,
                           const char* name,
                           AtmosphereDeviceSession* session,
                           content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit,
                           const AtmosphereModeSeed& seed) {
  if (!session || !session->device || !cam || !orbit || !name) {
    return 50;
  }

  const bool want_gpu = session->want_gpu;
  render::rhi::Device* device = session->device;
  HWND present_hwnd = session->present_hwnd;
  HWND owned_present_hwnd = session->owned_present_hwnd;
  const AtmosphereShowcaseLinger& linger = session->linger;
  const bool globe_flythrough = seed.globe_flythrough;
  const float globe_china_yaw = seed.globe_china_yaw;
  const float globe_china_pitch = seed.globe_china_pitch;

  HWND size_hwnd = owned_present_hwnd ? owned_present_hwnd : present_hwnd;
  uint32_t kW = kAtmosphereShowcaseW;
  uint32_t kH = kAtmosphereShowcaseH;
  atmosphere_hwnd_present_size(size_hwnd, &kW, &kH);
  // Default 3 warmup frames; raise via ATMOSPHERE_SHOWCASE_PRESENT_COUNT
  // for equal-profile benches vs leftover scene3d (same 640x480 HWND).
  int present_count = 3;
  if (const char* pc = base::switch_cstr("atmosphere-showcase-present-count")) {
    const int v = std::atoi(pc);
    if (v > 0 && v <= 600) {
      present_count = v;
    }
  }
  // Timed benches must not sleep between presents (50ms pump dominated wall).
  // present_pump_ms==0 also skips PeekMessage: draining the thread queue can
  // dispatch the main Browser map2d GDI paint (~100–200ms) and wreck equal-
  // profile ms/p even when Scene3dGpuPresent phases are single-digit.
  // Globe fly also skips pump — Map2d LayerStore AV under DX12 DispatchMessage.
  const int present_pump_ms =
      (present_count > 3 || globe_flythrough) ? 0 : 50;
  int presents = 0;

  bool early_bmp_ok = false;
  int dumped_globe_frames = 0;
  AtmosphereWarmupFail fail_ctx;
  fail_ctx.browser = &browser;
  fail_ctx.session = session;
  fail_ctx.cam = cam;
  fail_ctx.want_gpu = want_gpu;
  fail_ctx.globe_flythrough = globe_flythrough;
  fail_ctx.mode = mode;
  fail_ctx.name = name;
  fail_ctx.present_hwnd = present_hwnd;
  fail_ctx.owned_present_hwnd = owned_present_hwnd;
  fail_ctx.early_bmp_ok = &early_bmp_ok;

  PresentGpuWarmupOpts warm;
  warm.width_px = kW;
  warm.height_px = kH;
  warm.frames = 1;
  warm.pump_ms = present_pump_ms;
  warm.fail_log_prefix = "atmosphere-showcase";
  warm.mark = atmosphere_showcase_mark;
  warm.frame_mark = "present-warm";
  warm.on_fail = on_atmosphere_warmup_fail;
  warm.on_fail_user = &fail_ctx;
  // Warm one frame outside the timed window (pipeline / mesh upload).
  if (present_gpu_warmup(cam, device, warm) != 0) {
    return 52;
  }
  ++presents;
  // Globe: Google-Earth fly-in (space → high altitude) for HWND record + BMP.
  // Full descent to surface skim is done gently during timed linger only —
  // a long continuous dolly into the unit globe AVed under FlyCube.
  // DX12 flip-model HWNDs often BitBlt black — dump frames via the same
  // capture_hwnd_bmp path as the showcase BMP when HARNESS_RECORD=1.
  if (want_gpu && globe_flythrough) {
    const AtmosphereGlobeFlyResult fly = run_atmosphere_globe_fly_presents(
        mode, name, cam, orbit, device, present_hwnd, owned_present_hwnd,
        session->scene, globe_china_yaw, globe_china_pitch);
    presents += fly.presents_added;
    early_bmp_ok = fly.early_bmp_ok;
    dumped_globe_frames = fly.dumped_frames;
  } else if (want_gpu) {
    early_bmp_ok = capture_atmosphere_showcase_bmp(
        mode, name, cam, device, present_hwnd, owned_present_hwnd, want_gpu,
        globe_flythrough, session->scene);
  }
  LARGE_INTEGER qpf = {};
  LARGE_INTEGER t0 = {};
  LARGE_INTEGER t1 = {};
  QueryPerformanceFrequency(&qpf);
  QueryPerformanceCounter(&t0);
  fail_ctx.timed = true;
  PresentGpuWarmupOpts timed;
  timed.width_px = kW;
  timed.height_px = kH;
  timed.frames = present_count;
  timed.pump_ms = present_pump_ms;
  timed.fail_log_prefix = "atmosphere-showcase";
  // Equal-profile timed window: skip per-frame mark fopen/fflush (same class
  // of wall pollution as PeekMessage / map2d paint when pump_ms==0).
  timed.mark = (present_pump_ms == 0) ? nullptr : atmosphere_showcase_mark;
  timed.numbered_frame_marks = (present_pump_ms != 0);
  timed.on_fail = on_atmosphere_warmup_fail;
  timed.on_fail_user = &fail_ctx;
  if (present_gpu_warmup(cam, device, timed) != 0) {
    if (want_gpu && early_bmp_ok) {
      atmosphere_showcase_mark("pass-early-bmp");
      std::fprintf(stderr,
                   "atmosphere-showcase: PASS mode=%s (early BMP)\n", name);
      return 0;
    }
    return 52;
  }
  presents += present_count;
  QueryPerformanceCounter(&t1);
  const double present_ms =
      (qpf.QuadPart > 0)
          ? (1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) /
             static_cast<double>(qpf.QuadPart))
          : 0.0;
  const content::Scene3dPhaseSample phase = content::scene3d_last_phase_sample();
  char perf_leaf[MAX_PATH] = {};
  if (exe_capture_path_a(perf_leaf, MAX_PATH, "atmosphere-showcase-perf.json")) {
    if (FILE* pf = nullptr; fopen_s(&pf, perf_leaf, "wb") == 0 && pf) {
      std::fprintf(pf,
                   "{\"backend\":\"src-render\",\"mode\":\"%s\","
                   "\"present_count\":%d,\"present_ms\":%.3f,"
                   "\"ms_per_present\":%.3f,\"gpu\":%d,"
                   "\"mesh_ms\":%lld,\"sync_ms\":%lld,\"rebuild_ms\":%lld,"
                   "\"rebuild_count\":%d,\"ocean_prep_ms\":%lld,"
                   "\"record_ms\":%lld,\"present_swap_ms\":%lld}\n",
                   name, present_count, present_ms,
                   present_count > 0 ? present_ms / present_count : 0.0,
                   want_gpu ? 1 : 0,
                   static_cast<long long>(phase.mesh_ms),
                   static_cast<long long>(phase.sync_ms),
                   static_cast<long long>(phase.rebuild_ms), phase.rebuild_count,
                   static_cast<long long>(phase.ocean_prep_ms),
                   static_cast<long long>(phase.record_ms),
                   static_cast<long long>(phase.present_ms));
      std::fclose(pf);
    }
  }
  atmosphere_showcase_mark("present-ok");
  std::fprintf(stderr,
               "atmosphere-showcase: presented %d frames %ux%u "
               "present_ms=%.2f ms/p=%.2f "
               "phase mesh=%lld sync=%lld rebuild=%lld(n=%d) ocean=%lld "
               "record=%lld swap=%lld\n",
               presents, kW, kH, present_ms,
               present_count > 0 ? present_ms / present_count : 0.0,
               static_cast<long long>(phase.mesh_ms),
               static_cast<long long>(phase.sync_ms),
               static_cast<long long>(phase.rebuild_ms), phase.rebuild_count,
               static_cast<long long>(phase.ocean_prep_ms),
               static_cast<long long>(phase.record_ms),
               static_cast<long long>(phase.present_ms));

  bool bmp_signal_ok = !want_gpu || early_bmp_ok;
  if (linger.until_close || linger.ms > 0) {
    const AtmospherePresentLingerResult linger_out =
        run_atmosphere_present_linger(
            mode, name, cam, orbit, device, present_hwnd, owned_present_hwnd,
            linger, globe_flythrough, globe_china_yaw, globe_china_pitch,
            early_bmp_ok, dumped_globe_frames);
    bmp_signal_ok = linger_out.bmp_signal_ok;
  } else if (want_gpu && !bmp_signal_ok) {
    bmp_signal_ok = capture_atmosphere_showcase_bmp(
        mode, name, cam, device, present_hwnd, owned_present_hwnd, want_gpu,
        globe_flythrough, session->scene);
  }

  // Skip abandon_mesh after live FlyCube present — peer world3d/stormsurge:
  // DX12 present + abandon remaps heap (hang / AV before PASS mark).
  if (!want_gpu) {
    cam->abandon_mesh();
  }
  // Intentionally skip device->shutdown() — FlyCube DX12 teardown after a live
  // present has heap-corrupted ExitProcess (peer stormsurge / world3d). Leak
  // the Device* the same way DrawHost does after a live session.
  finish_atmosphere_device_session(browser, session, /*shutdown_device=*/false);
  if (want_gpu && !bmp_signal_ok) {
    atmosphere_showcase_mark("bmp-fail");
    std::fprintf(stderr, "atmosphere-showcase: FAIL mode=%s (exit 54)\n", name);
    return 54;
  }
  atmosphere_showcase_mark("pass");
  std::fprintf(stderr, "atmosphere-showcase: PASS mode=%s\n", name);
  return 0;
}

}  // namespace detail
}  // namespace app

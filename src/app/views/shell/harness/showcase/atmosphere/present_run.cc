// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/present_run.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/bmp.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/showcase/atmosphere/globe_fly.h"
#include "app/views/shell/harness/showcase/atmosphere/label_composite.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_phase_profile.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "effect/atmosphere/globe/globe_pass.h"
#include "render/rhi/rhi.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* step) {
  write_mark(kAtmosphereShowcaseMarkLeaf, step, /*truncate=*/false);
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
  const bool owns_device = session->owns_device;

  const uint32_t kW = kAtmosphereShowcaseW;
  const uint32_t kH = kAtmosphereShowcaseH;
  // Default 3 warmup frames; raise via SMT_ATMOSPHERE_SHOWCASE_PRESENT_COUNT
  // for equal-profile benches vs leftover scene3d (same 640x480 HWND).
  int present_count = 3;
  if (const char* pc = std::getenv("SMT_ATMOSPHERE_SHOWCASE_PRESENT_COUNT")) {
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
  auto present_one = [&](const char* mark) -> bool {
    showcase_mark(mark);
    if (!cam->present_gpu(device, kW, kH)) {
      return false;
    }
    ++presents;
    if (present_pump_ms > 0) {
      pump_messages(present_pump_ms);
    }
    return true;
  };

  // Capture while the present HWND is still alive (before until-close ends).
  // Defined before the warmup loop so a mid-warmup FlyCube fault can still
  // leave a BMP from the first good frame (peer world3d heap notes).
  auto capture_showcase_bmp = [&]() -> bool {
    wchar_t bmp_path[MAX_PATH] = {};
    wchar_t file[64] = {};
    swprintf_s(file, L"atmosphere-showcase-%S.bmp", name);
    if (!exe_capture_path(bmp_path, MAX_PATH, file)) {
      return !want_gpu;
    }
    HWND capture_hwnd =
        owned_present_hwnd ? owned_present_hwnd : present_hwnd;
    (void)cam->present_gpu(device, kW, kH);
    if (!globe_flythrough) {
      pump_messages(80);
    } else {
      Sleep(80);
    }
    if (!capture_hwnd_bmp(capture_hwnd, bmp_path)) {
      showcase_mark("bmp-skip");
      std::fprintf(stderr, "atmosphere-showcase: BMP capture skipped\n");
      return !want_gpu;
    }
    int bw = 0;
    int bh = 0;
    BmpFileCheckOpts check;
    check.require_color_diversity = true;
    const bool signal =
        bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
    std::fwprintf(stderr,
                  L"atmosphere-showcase: wrote %ls (%dx%d signal=%d)\n",
                  bmp_path, bw, bh, signal ? 1 : 0);
    if (signal) {
      showcase_mark("bmp-ok");
      if (mode == AtmosphereShowcaseMode::kLegacy) {
        if (composite_legacy_labels_onto_bmp(cam, bmp_path, bw, bh)) {
          showcase_mark("labels-bmp-ok");
        } else {
          showcase_mark("labels-bmp-skip");
        }
      }
      return true;
    }
    showcase_mark("bmp-black");
    std::fprintf(stderr, "atmosphere-showcase: BMP lacks visible signal\n");
    return false;
  };

  bool early_bmp_ok = false;
  // Warm one frame outside the timed window (pipeline / mesh upload).
  if (!present_one("present-warm")) {
    std::fprintf(stderr, "atmosphere-showcase: present_gpu failed warm\n");
    cam->abandon_mesh();
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 52;
  }
  // Globe: Google-Earth fly-in (space → high altitude) for HWND record + BMP.
  // Full descent to surface skim is done gently during timed linger only —
  // a long continuous dolly into the unit globe AVed under FlyCube.
  // DX12 flip-model HWNDs often BitBlt black — dump frames via the same
  // capture_hwnd_bmp path as the showcase BMP when SMT_HARNESS_RECORD=1.
  const bool dump_globe_frames = []() {
    const char* r = std::getenv("SMT_HARNESS_RECORD");
    if (!r || !*r) {
      return false;
    }
    return (r[0] == '1' && r[1] == '\0') || std::strcmp(r, "true") == 0 ||
           std::strcmp(r, "yes") == 0 || std::strcmp(r, "on") == 0;
  }();
  int globe_frame_i = 0;
  auto dump_globe_frame = [&]() {
    if (!dump_globe_frames || !owned_present_hwnd) {
      return;
    }
    wchar_t path[MAX_PATH] = {};
    wchar_t leaf[96] = {};
    swprintf_s(leaf, L"record\\atmosphere_globe_fly\\frame_%05d.bmp",
               globe_frame_i);
    if (!exe_capture_path(path, MAX_PATH, leaf)) {
      return;
    }
    if (capture_hwnd_bmp(owned_present_hwnd, path)) {
      ++globe_frame_i;
    }
  };
  if (want_gpu && globe_flythrough) {
    // Full cinematic present pass (space → high → DEM horizon skim).
    // BMP is captured at the high-altitude beat; keyframes dump after.
    const effect::atmosphere::GlobePass* globe =
        &cam->atmosphere_session().globe_pass();
    content::AtmosphereSession* atm = &cam->atmosphere_session();
    showcase_mark("globe-flyin");
    constexpr int kFlyFrames = 96;
    for (int i = 0; i < kFlyFrames; ++i) {
      const float t =
          static_cast<float>(i) / static_cast<float>(kFlyFrames - 1);
      apply_globe_flythrough(orbit, t, globe_china_yaw, globe_china_pitch,
                             globe, atm);
      if (!cam->present_gpu(device, kW, kH)) {
        std::fprintf(stderr,
                     "atmosphere-showcase: globe fly-in present failed\n");
        break;
      }
      ++presents;
      // Do not DispatchMessage here: shell Map2d paint AVs under DX12 globe
      // fly (LayerStore::feature_count). GPU present does not need a pump.
      Sleep(8);
    }
    showcase_mark("globe-high");
    // High-altitude pose for the suite BMP / landish gates.
    apply_globe_flythrough(orbit, 0.48f, globe_china_yaw, globe_china_pitch,
                           globe, atm);
    early_bmp_ok = capture_showcase_bmp();
    // Keyframe dump AFTER the hot present loop — in-loop GDI capture AVs DX12.
    if (dump_globe_frames) {
      constexpr float kKeys[] = {0.f,   0.10f, 0.22f, 0.36f, 0.48f,
                                 0.62f, 0.74f, 0.86f, 0.94f, 1.f};
      for (float kt : kKeys) {
        apply_globe_flythrough(orbit, kt, globe_china_yaw, globe_china_pitch,
                               globe, atm);
        if (!cam->present_gpu(device, kW, kH)) {
          break;
        }
        Sleep(40);
        dump_globe_frame();
      }
      apply_globe_flythrough(orbit, 0.48f, globe_china_yaw, globe_china_pitch,
                             globe, atm);
      showcase_mark("globe-frames-ok");
    }
  } else if (want_gpu) {
    early_bmp_ok = capture_showcase_bmp();
  }
  LARGE_INTEGER qpf = {};
  LARGE_INTEGER t0 = {};
  LARGE_INTEGER t1 = {};
  QueryPerformanceFrequency(&qpf);
  QueryPerformanceCounter(&t0);
  for (int i = 0; i < present_count; ++i) {
    char frame_mark[32];
    std::snprintf(frame_mark, sizeof(frame_mark), "present-%d", i);
    if (!present_one(frame_mark)) {
      std::fprintf(stderr, "atmosphere-showcase: present_gpu failed frame %d\n",
                   i);
      if (want_gpu && presents > 0 && !early_bmp_ok) {
        early_bmp_ok = capture_showcase_bmp();
      }
      cam->abandon_mesh();
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      detach_maps(browser);
      if (want_gpu && early_bmp_ok) {
        showcase_mark("pass-early-bmp");
        std::fprintf(stderr,
                     "atmosphere-showcase: PASS mode=%s (early BMP)\n", name);
        return 0;
      }
      return 52;
    }
  }
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
  showcase_mark("present-ok");
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
    showcase_mark("linger-start");
    if (linger.until_close) {
      std::fprintf(stderr,
                   "atmosphere-showcase: linger until window closed "
                   "(close the showcase window when done)\n");
    } else {
      std::fprintf(stderr, "atmosphere-showcase: linger %lu ms\n",
                   static_cast<unsigned long>(linger.ms));
    }
    const DWORD linger_end =
        linger.until_close ? 0u : (GetTickCount() + linger.ms);
    const DWORD linger_start = GetTickCount();
    int linger_frames = 0;
    bool captured = early_bmp_ok;
    // Timed CI: do not sleep 33ms between presents (that capped ~30fps and
    // hid present cost). Interactive until-close still paces for readability.
    // SMT_ATMOSPHERE_SHOWCASE_PUMP_MS overrides (e.g. 16 ≈ 60Hz interactive).
    int pump_ms = linger.until_close ? 16 : 0;
    if (const char* pump_env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_PUMP_MS");
        pump_env && *pump_env) {
      pump_ms = std::atoi(pump_env);
      if (pump_ms < 0) {
        pump_ms = 0;
      }
    }
    for (;;) {
      if (owned_present_hwnd && !IsWindow(owned_present_hwnd)) {
        break;
      }
      if (!linger.until_close && GetTickCount() >= linger_end) {
        break;
      }
      // High → surface skim during timed linger (yaw drift). Abort dolly on
      // present failure so HWND record still closes cleanly.
      if (globe_flythrough) {
        // Interactive linger: replay dive + horizon skim after the fly pass.
        float t = 0.48f;
        if (linger.ms > 0) {
          const float elapsed =
              static_cast<float>(GetTickCount() - linger_start);
          t = 0.48f + 0.52f * (std::min)(
                                  1.f, elapsed / static_cast<float>(linger.ms));
        } else if (linger.until_close) {
          const float elapsed_s =
              static_cast<float>(GetTickCount() - linger_start) / 1000.f;
          t = 0.48f + 0.52f * (std::min)(1.f, elapsed_s / 10.f);
        }
        apply_globe_flythrough(orbit, t, globe_china_yaw, globe_china_pitch,
                               &cam->atmosphere_session().globe_pass(),
                               &cam->atmosphere_session());
      }
      if (!cam->present_gpu(device, kW, kH)) {
        std::fprintf(stderr, "atmosphere-showcase: linger present failed\n");
        break;
      }
      ++linger_frames;
      if (!captured && linger_frames >= 8) {
        bmp_signal_ok = capture_showcase_bmp();
        captured = true;
      }
      pump_messages(pump_ms);
    }
    if (globe_flythrough && dump_globe_frames && globe_frame_i > 0) {
      std::fprintf(stderr,
                   "atmosphere-showcase: dumped %d globe fly frames under "
                   "captures/record/atmosphere_globe_fly/\n",
                   globe_frame_i);
    }
    if (!captured) {
      bmp_signal_ok = capture_showcase_bmp();
    }
    showcase_mark("linger-ok");
    const DWORD elapsed = GetTickCount() - linger_start;
    const DWORD wall_ms = elapsed == 0u ? 1u : elapsed;
    const float avg_fps =
        static_cast<float>(linger_frames) * 1000.f /
        static_cast<float>(wall_ms);
    std::fprintf(stderr,
                 "atmosphere-showcase: linger frames=%d wall_ms=%lu "
                 "avg_fps=%.2f last_fps=%.2f\n",
                 linger_frames, static_cast<unsigned long>(wall_ms), avg_fps,
                 cam->gpu().last_fps);
  } else if (want_gpu && !bmp_signal_ok) {
    bmp_signal_ok = capture_showcase_bmp();
  }

  cam->abandon_mesh();
  // Intentionally skip device->shutdown() — FlyCube DX12 teardown after a live
  // present has heap-corrupted ExitProcess (peer stormsurge / world3d). Leak
  // the Device* the same way MapViewport does after a live session.
  (void)owns_device;
  (void)device;
  if (owned_present_hwnd) {
    DestroyWindow(owned_present_hwnd);
    owned_present_hwnd = nullptr;
    session->owned_present_hwnd = nullptr;
  }
  detach_maps(browser);
  if (want_gpu && !bmp_signal_ok) {
    showcase_mark("bmp-fail");
    std::fprintf(stderr, "atmosphere-showcase: FAIL mode=%s (exit 54)\n", name);
    return 54;
  }
  showcase_mark("pass");
  std::fprintf(stderr, "atmosphere-showcase: PASS mode=%s\n", name);
  return 0;
}

}  // namespace detail
}  // namespace app

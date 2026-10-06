// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/present/present_linger.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/atmosphere/capture/capture.h"
#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scenario/atmosphere/common/progress.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include "base/process/switches.h"

namespace plugin {
namespace detail {

AtmospherePresentLingerResult run_atmosphere_present_linger(
    AtmosphereShowcaseMode mode,
    const char* mode_name,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    render::rhi::Device* device,
    HWND present_hwnd,
    HWND owned_present_hwnd,
    const AtmosphereShowcaseLinger& linger,
    bool globe_flythrough,
    float china_yaw,
    float china_pitch,
    bool early_bmp_ok,
    int dumped_globe_frames) {
  AtmospherePresentLingerResult out;
  out.bmp_signal_ok = early_bmp_ok;
  if (!cam || !orbit || !device || !mode_name) {
    return out;
  }
  if (!linger.until_close && linger.ms == 0) {
    return out;
  }

  atmosphere_showcase_mark("linger-start");
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
  bool captured = early_bmp_ok;
  // Timed CI: do not sleep 33ms between presents (that capped ~30fps and
  // hid present cost). Interactive until-close still paces for readability.
  // ATMOSPHERE_SHOWCASE_PUMP_MS overrides (e.g. 16 ≈ 60Hz interactive).
  int pump_ms = linger.until_close ? 16 : 0;
  if (const char* pump_env = base::switch_cstr("atmosphere-showcase-pump-ms");
      pump_env && *pump_env) {
    pump_ms = std::atoi(pump_env);
    if (pump_ms < 0) {
      pump_ms = 0;
    }
  }
  uint32_t pw = kAtmosphereShowcaseW;
  uint32_t ph = kAtmosphereShowcaseH;
  atmosphere_hwnd_present_size(
      owned_present_hwnd ? owned_present_hwnd : present_hwnd, &pw, &ph);
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
      plugin::apply_world3d_globe_flythrough(orbit, t, china_yaw, china_pitch,
                             &cam->atmosphere_session().globe_pass(),
                             &cam->atmosphere_session());
    }
    if (!cam->present_gpu(device, pw, ph)) {
      std::fprintf(stderr, "atmosphere-showcase: linger present failed\n");
      break;
    }
    ++out.frames;
    if (!captured && out.frames >= 8) {
      out.bmp_signal_ok = capture_atmosphere_showcase_bmp(
          mode, mode_name, cam, device, present_hwnd, owned_present_hwnd,
          /*want_gpu=*/true, globe_flythrough, nullptr);
      captured = true;
    }
    if (HarnessShell* shell = atmosphere_showcase_shell()) {
      shell->pump(pump_ms);
    }
  }
  if (globe_flythrough && dumped_globe_frames > 0) {
    std::fprintf(stderr,
                 "atmosphere-showcase: dumped %d globe fly frames under "
                 "captures/record/atmosphere_globe_fly/\n",
                 dumped_globe_frames);
  }
  if (!captured) {
    out.bmp_signal_ok = capture_atmosphere_showcase_bmp(
        mode, mode_name, cam, device, present_hwnd, owned_present_hwnd,
        /*want_gpu=*/true, globe_flythrough, nullptr);
  }
  atmosphere_showcase_mark("linger-ok");
  const DWORD elapsed = GetTickCount() - linger_start;
  const DWORD wall_ms = elapsed == 0u ? 1u : elapsed;
  const float avg_fps =
      static_cast<float>(out.frames) * 1000.f / static_cast<float>(wall_ms);
  std::fprintf(stderr,
               "atmosphere-showcase: linger frames=%d wall_ms=%lu "
               "avg_fps=%.2f last_fps=%.2f\n",
               out.frames, static_cast<unsigned long>(wall_ms), avg_fps,
               cam->gpu().last_fps);
  return out;
}

}  // namespace detail
}  // namespace plugin

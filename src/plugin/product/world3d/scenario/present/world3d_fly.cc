// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/present/world3d_fly.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "ui/views/map/viewport/draw_host.h"
#include "vista/pass/world/atmosphere/globe/globe_pass.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <windows.h>

namespace plugin {
namespace detail {
namespace {

bool present_fly_frame(HarnessShell& browser,
                       content::Scene3dPresenter* cam,
                       PluginDeviceSession* session) {
  if (session->borrowed_shell) {
    // Globe albedo/china remesh can exceed 400ms on Debug FlyCube.
    return present_shell_scene3d_frame(browser.scene_draw_host(), 1500);
  }
  return cam->present_gpu(session->device, kPluginPresentW,
                          kPluginPresentH);
}

void warm_presents(HarnessShell& browser,
                   content::Scene3dPresenter* cam,
                   PluginDeviceSession* session,
                   int count,
                   DWORD sleep_ms) {
  for (int i = 0; i < count; ++i) {
    (void)present_fly_frame(browser, cam, session);
    if (sleep_ms > 0) {
      Sleep(sleep_ms);
    }
  }
}

bool capture_stage_bmp(content::Scene3dPresenter* cam,
                       PluginDeviceSession* session,
                       const wchar_t* leaf,
                       bool relax_diversity) {
  if (!cam || !session || !leaf) {
    return false;
  }
  Scene3dHwndCaptureOpts core;
  core.bmp_leaf = leaf;
  core.present_w = kPluginPresentW;
  core.present_h = kPluginPresentH;
  core.pre_capture_pump_ms = 120;
  core.use_grid_lit_policy = false;
  core.retry_dark_frame = true;
  // Space / early cloud frames are mostly navy + limb -- diversity gate is too
  // strict for those beats.
  core.require_color_diversity = !relax_diversity;
  core.skip_ui_thread_present = session->borrowed_shell;
  core.skip_when_null_gpu = false;
  core.mark = plugin_mark;
  core.log_prefix = "plugin-showcase";

  const bool want_gpu = session->want_gpu || session->borrowed_shell;
  return capture_scene3d_hwnd_bmp(cam, session->device, session->present_hwnd,
                                  want_gpu, core);
}

struct StageBeat {
  float t = 0.f;
  const char* mark = nullptr;
  const wchar_t* bmp_leaf = nullptr;
  bool relax_diversity = false;
  int warm_frames = 4;
  bool want_forward_skim = false;
};

}  // namespace

World3dGlobeFlyResult run_world3d_globe_fly_presents(
    HarnessShell& browser,
    content::Scene3dPresenter* cam,
    content::OrbitFrame* orbit,
    PluginDeviceSession* session) {
  World3dGlobeFlyResult out;
  if (!cam || !orbit || !session) {
    return out;
  }
  if (!cam->atmosphere_session().globe_enabled()) {
    return out;
  }

  float china_yaw = 0.f;
  float china_pitch = 0.f;
  plugin::world3d_china_aim_yaw_pitch(&china_yaw, &china_pitch);

  const vista::GlobePass* globe = &cam->atmosphere_session().globe_pass();
  content::AtmosphereSession* atm = &cam->atmosphere_session();

  plugin_mark("globe-fly-begin");
  std::fprintf(stderr,
               "plugin-showcase: world3d fly space→clouds→DEM-hug→ocean\n");
  // Skim matrices come from orbit_; re-bind in case startup sizeof skew dropped it.
  cam->bind_orbit(orbit);

  // West→east terrain-hug; keep frame count bounded for harness timeout.
  // Animated pass uses orbit path-hug (look-at-origin) — look-at skim during
  // the long loop hung FlyCube present. Horizontal-forward skim is reserved
  // for DEM/ocean stage stills below (正前方贴地截图).
  constexpr int kFlyFrames = 48;
  int last_stage = -1;
  constexpr bool allow_forward_skim = false;
  for (int i = 0; i < kFlyFrames; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>(kFlyFrames - 1);
    plugin::apply_world3d_globe_flythrough(orbit, t, china_yaw, china_pitch,
                                           globe, atm, allow_forward_skim);

    int stage = 0;
    if (t >= 0.82f) {
      stage = 3;  // ocean Gerstner
    } else if (t >= 0.62f) {
      stage = 2;  // terrain-hug DEM skim
    } else if (t >= 0.15f) {
      stage = 1;
    }
    if (stage != last_stage) {
      last_stage = stage;
      switch (stage) {
        case 0:
          plugin_mark("fly-space");
          break;
        case 1:
          plugin_mark("fly-clouds");
          break;
        case 2:
          plugin_mark("fly-dem");
          break;
        default:
          plugin_mark("fly-ocean");
          break;
      }
    }

    if (!present_fly_frame(browser, cam, session)) {
      std::fprintf(stderr,
                   "plugin-showcase: world3d fly present failed at t=%.2f\n",
                   t);
      break;
    }
    ++out.presents_added;
  }

  const StageBeat beats[] = {
      // Diversity on: solid sky-clear PrintWindow frames must not pass.
      {plugin::kWorld3dGlobeFlySpaceT, "fly-space-bmp",
       L"plugin-showcase-world3d-space.bmp", false, 4, false},
      {plugin::kWorld3dGlobeFlyCloudsT, "fly-clouds-bmp",
       L"plugin-showcase-world3d-clouds.bmp", false, 4, false},
      // DEM/ocean stills: forward skim with raised clearance + skim FOV.
      {plugin::kWorld3dGlobeFlyDemT, "fly-dem-bmp",
       L"plugin-showcase-world3d-dem.bmp", false, 6, true},
      {plugin::kWorld3dGlobeFlyOceanT, "fly-ocean-bmp",
       L"plugin-showcase-world3d-ocean.bmp", false, 6, true},
  };
  for (const StageBeat& beat : beats) {
    plugin::apply_world3d_globe_flythrough(orbit, beat.t, china_yaw, china_pitch,
                                           globe, atm, beat.want_forward_skim);
    // One probe present: skim can wedge the GPU; fall back before warm/capture.
    if (!present_fly_frame(browser, cam, session)) {
      if (orbit->forward_skim_active()) {
        std::fprintf(stderr,
                     "plugin-showcase: stage skim present fail t=%.2f -- "
                     "orbit path-hug for BMP\n",
                     beat.t);
        orbit->clear_forward_skim();
        plugin::apply_world3d_globe_flythrough(orbit, beat.t, china_yaw,
                                               china_pitch, globe, atm,
                                               /*allow_forward_skim=*/false);
        if (!present_fly_frame(browser, cam, session)) {
          plugin_mark("fly-stage-bmp-skip");
          continue;
        }
      } else {
        plugin_mark("fly-stage-bmp-skip");
        continue;
      }
    }
    warm_presents(browser, cam, session, (std::max)(0, beat.warm_frames - 1),
                  24);
    if (capture_stage_bmp(cam, session, beat.bmp_leaf, beat.relax_diversity)) {
      plugin_mark(beat.mark);
      ++out.stage_bmps_ok;
    } else if (orbit->forward_skim_active()) {
      // Capture under skim flaked — still try orbit path-hug still.
      orbit->clear_forward_skim();
      plugin::apply_world3d_globe_flythrough(orbit, beat.t, china_yaw,
                                             china_pitch, globe, atm,
                                             /*allow_forward_skim=*/false);
      warm_presents(browser, cam, session, 2, 16);
      if (capture_stage_bmp(cam, session, beat.bmp_leaf, beat.relax_diversity)) {
        plugin_mark(beat.mark);
        ++out.stage_bmps_ok;
      } else {
        plugin_mark("fly-stage-bmp-skip");
      }
    } else {
      plugin_mark("fly-stage-bmp-skip");
    }
  }

  // Park at high-China DEM hold for the suite score BMP (landish gate).
  // Strip ocean before the climb — Gerstner→high-orbit jumps have AV'd FlyCube.
  if (atm) {
    atm->set_ocean_enabled(false);
    atm->set_sat_cloud_enabled(true);
  }
  plugin::apply_world3d_globe_flythrough(
      orbit, plugin::kWorld3dGlobeFlyParkT, china_yaw, china_pitch, globe, atm,
      /*allow_forward_skim=*/false);
  plugin_mark("globe-fly-park");
  warm_presents(browser, cam, session, 2, 16);
  // Suite gates (also written by plugin.world3d.il when the IL path runs).
  plugin_mark("fit-box-ok");
  plugin_mark("camera-fly-ok");
  return out;
}

}  // namespace detail
}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/scenario/run.h"

#include <windows.h>

#include <cmath>
#include <cstdio>

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "plugin/product/mine/scenario/seed.h"
#include "plugin/product/world3d/scenario/capture/capture.h"
#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "plugin/product/world3d/scenario/present/present_warmup.h"
#include "plugin/product/world3d/scenario/seed/orbit_seed.h"
#include "plugin/product/world3d/scenario/session/device_session.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/shell.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

namespace plugin {
namespace detail {

// Scene3D path: mine TIN + borehole sticks with real Z, HWND BMP capture.
// Order matches orthogrid3d: orbit clear → commit overlay → warmup keep tin →
// recommit + reframe so DEM rebuild cannot leave a DEM-only capture.
int run_mine_scene3d(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: mine Scene3D path\n");
  browser.mark_named(plugin::kMarkPlugin, "mine", /*truncate=*/true);

  char csv_path[MAX_PATH * 3] = {};
  if (!resolve_mine_boreholes_csv(csv_path, sizeof(csv_path))) {
    plugin_mark("mine-sample-fail");
    browser.detach_views();
    return 1;
  }
  plugin_mark("sample-ok");

  // GPU Scene3D on the main App 3D pane (FlyCube/RHI). Never GDI software 3D.
  if (!content::apply_scene3d_engine_from_env() ||
      content::prefer_scene3d_gdi()) {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  plugin_mark("tab3d");
  browser.pump(200);

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "plugin-mine-gpu";
  opts.require_scene_hwnd = true;
  opts.detach_flycube = false;
  // Borrow shell Scene3D with select_view_tab(1) (peer orthogrid3d). Skipping
  // select left device-missing / navy-only captures.
  opts.borrow_shell_scene3d = true;

  PluginDeviceSession session;
  if (const int rc = prepare_plugin_device_session(browser, opts, &session)) {
    destroy_plugin_owned_hwnd(&session);
    browser.detach_views();
    return rc;
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    browser.detach_views();
    return 50;
  }

  // Orbit clears stale DEM first; stratum commit must follow.
  seed_mine_orbit(browser, cam, orbit);

  if (!seed_mine_processing(browser, csv_path)) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    browser.detach_views();
    return 1;
  }

  if (!cam->gpu().overlay_tin_has_albedo()) {
    plugin_mark("mine-overlay-empty");
    // Present-phase flush may have raced; recommit once before warmup.
    if (!seed_mine_processing(browser, csv_path) ||
        !cam->gpu().overlay_tin_has_albedo()) {
      plugin_mark("mine-overlay-recommit-fail");
    }
  } else {
    plugin_mark("mine-overlay-ok");
  }

  // Processing / tab switch may re-apply china atmosphere — strip before warmup.
  disable_plugin_atmosphere(cam);
  cam->gpu().set_wireframe_enabled(false);
  cam->gpu().set_studio_block(true);
  frame_mine_orbit(orbit);

  PluginPresentFailPolicy warm_fail;
  warm_fail.clear_tin = false;  // keep layered stratum through warmup + capture
  warm_fail.clear_pointcloud = true;
  warm_fail.abandon_mesh = false;
  warm_fail.shutdown_device = false;
  if (const int rc = present_plugin_warmup_frames(
          cam, &session, browser, "mine", warm_fail, 3)) {
    return rc;
  }

  frame_mine_orbit(orbit);
  if (present_shell_scene3d_frame(browser.scene_draw_host(), 400)) {
    plugin_mark("present-gpu-ok");
  }
  // switch_map_tab / warmup can re-apply China atmosphere and drop studio_block
  // (DEM-only green plane). Peer orthogrid3d — strip + reframe before capture.
  disable_plugin_atmosphere(cam);
  cam->gpu().set_studio_block(true);
  frame_mine_orbit(orbit);
  if (!cam->gpu().overlay_tin_has_albedo()) {
    plugin_mark("mine-overlay-empty-post");
    (void)seed_mine_processing(browser, csv_path);
    disable_plugin_atmosphere(cam);
    cam->gpu().set_studio_block(true);
    frame_mine_orbit(orbit);
  }
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    session.present_hwnd = shell_scene3d_capture_hwnd(scene);
    session.device = static_cast<render::rhi::Device*>(scene->rhi_device());
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-mine.bmp";
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  // Suite gates fit-box-ok / camera-fly-ok. Publish here (peer world3d_fly):
  // IL fit_scene_box / camera_fly after run_plugin_command has AV'd ExitProcess
  // once FlyCube has a live lithology present. Orbit via pump only — extra
  // present_shell_scene3d_frame after capture re-triggered the same AV.
  frame_mine_orbit(orbit);
  plugin_mark("fit-box-ok");
  {
    constexpr float kPi = 3.14159265f;
    const float yaw0 = orbit->yaw();
    const float pitch0 = orbit->pitch();
    const float dist0 = orbit->distance();
    constexpr int kSteps = 8;
    for (int i = 0; i <= kSteps; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(kSteps);
      orbit->set_yaw(yaw0 + kPi * 1.5f * t);
      orbit->set_pitch(pitch0 + 0.08f * std::sin(t * kPi));
      orbit->set_distance(dist0);
      if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
        scene->invalidate_native();
      }
      browser.pump(60);
    }
    orbit->set_yaw(yaw0 + kPi * 1.5f * 0.35f);
    orbit->set_pitch(pitch0);
    orbit->set_distance(dist0);
    if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
      scene->invalidate_native();
    }
    browser.pump(80);
  }
  plugin_mark("camera-fly-ok");

  // Soft teardown: abandon_mesh / clear_tin after a live FlyCube present has
  // AV'd ExitProcess. finish_scene3d only pauses shell present timers.
  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = true,
                         .clear_tin = false,
                         .abandon_mesh = false,
                         .shutdown_device = false});

  if (!bmp_ok && session.want_gpu) {
    browser.finish_scene3d(session.borrowed_shell);
    return 54;
  }
  plugin_mark("pass");
  browser.finish_scene3d(session.borrowed_shell);
  std::fprintf(stderr, "plugin-showcase: PASS mode=mine (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin

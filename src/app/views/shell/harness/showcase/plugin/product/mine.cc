// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/mine.h"

#include <windows.h>

#include <cstdio>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/capture/capture.h"
#include "app/views/shell/harness/showcase/plugin/session/device_session.h"
#include "app/views/shell/harness/showcase/plugin/seed/mine_seed.h"
#include "app/views/shell/harness/showcase/plugin/seed/orbit_seed.h"
#include "app/views/shell/harness/showcase/plugin/common/plugin_io.h"
#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"

namespace app {
namespace detail {

// Scene3D path: mine TIN + borehole sticks with real Z, HWND BMP capture.
// Order matches orthogrid3d: orbit clear → commit overlay → warmup keep tin →
// recommit + reframe so DEM rebuild cannot leave a DEM-only capture.
int run_mine_scene3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: mine Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "mine", /*truncate=*/true);

  char csv_path[MAX_PATH * 3] = {};
  if (!resolve_mine_boreholes_csv(csv_path, sizeof(csv_path))) {
    plugin_showcase_mark("mine-sample-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("sample-ok");

  // GPU Scene3D on the main App 3D pane (FlyCube/RHI). Never GDI software 3D.
  if (!content::apply_scene3d_engine_from_env() ||
      content::prefer_scene3d_gdi()) {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  plugin_showcase_mark("tab3d");
  pump_messages(200);

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "plugin-mine-gpu";
  opts.require_scene_hwnd = true;
  opts.detach_flycube = true;

  PluginDeviceSession session;
  if (const int rc = prepare_plugin_device_session(browser, opts, &session)) {
    destroy_plugin_owned_hwnd(&session);
    detach_maps(browser);
    return rc;
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    detach_maps(browser);
    return 50;
  }

  // Orbit clears stale DEM first; stratum commit must follow.
  seed_mine_orbit(browser, cam, orbit);

  if (!seed_mine_processing(browser, csv_path)) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    detach_maps(browser);
    return 1;
  }

  cam->gpu().set_wireframe_enabled(false);

  PluginPresentFailPolicy warm_fail;
  warm_fail.clear_tin = false;  // keep layered stratum through warmup + capture
  warm_fail.clear_pointcloud = true;
  warm_fail.abandon_mesh = false;
  warm_fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(
          cam, &session, browser, "mine", warm_fail, 3)) {
    return rc;
  }

  frame_mine_orbit(orbit);
  if (present_shell_scene3d_frame(browser.scene_draw_host(), 400)) {
    plugin_showcase_mark("present-gpu-ok");
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-mine.bmp";
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = true,
                         .clear_tin = true,
                         .abandon_mesh = true,
                         .shutdown_device = false});

  if (!bmp_ok && session.want_gpu) {
    finish_scene3d_showcase(browser, session.borrowed_shell);
    return 54;
  }
  plugin_showcase_mark("pass");
  finish_scene3d_showcase(browser, session.borrowed_shell);
  std::fprintf(stderr, "plugin-showcase: PASS mode=mine (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app

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
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"
#include "app/views/shell/harness/showcase/plugin/seed/mine_seed.h"
#include "app/views/shell/harness/showcase/plugin/seed/orbit_seed.h"
#include "app/views/shell/harness/showcase/plugin/session/device_session.h"
#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

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

  // Owned present HWND is the Scene3D capture target (peer world3d). Skip
  // select_map_tab(2): switch_map_tab stereo release AVs when leftover GL
  // destroy_ is stale under FlyCube-default sessions.
  plugin_showcase_mark("tab3d");
  pump_messages(200);

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "SMT_PLUGIN_MINE_GPU";
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

  cam->gpu().set_wireframe_enabled(true);

  PluginPresentFailPolicy warm_fail;
  warm_fail.clear_tin = false;  // keep purple stratum through warmup + capture
  warm_fail.clear_pointcloud = false;  // keep amber borehole sticks
  warm_fail.abandon_mesh = false;
  warm_fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(
          cam, &session, browser, "mine", warm_fail)) {
    return rc;
  }

  // Re-commit overlay after DEM rebuild so GDI capture sees purple + amber.
  if (!seed_mine_processing(browser, csv_path)) {
    plugin_showcase_mark("mine-recommit-fail");
  }
  frame_mine_orbit(orbit);
  for (int i = 0; i < 2; ++i) {
    (void)cam->present_gpu(session.device, kPluginShowcasePresentW,
                           kPluginShowcasePresentH);
    pump_messages(30);
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-mine.bmp";
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = true,
                         .clear_tin = true,
                         .abandon_mesh = true,
                         .shutdown_device = true});

  if (!bmp_ok && session.want_gpu) {
    detach_maps(browser);
    return 54;
  }
  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=mine (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app

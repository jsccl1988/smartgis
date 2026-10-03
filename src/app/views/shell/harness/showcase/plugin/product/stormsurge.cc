// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/stormsurge.h"

#include <windows.h>

#include <cstdio>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/capture/capture.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"
#include "app/views/shell/harness/showcase/plugin/seed/orbit_seed.h"
#include "app/views/shell/harness/showcase/plugin/seed/stormsurge_seed.h"
#include "app/views/shell/harness/showcase/plugin/session/device_session.h"
#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

namespace app {
namespace detail {

// Scene3D path: stormsurge water TIN overlay (mask stays map2d), HWND BMP.
int run_stormsurge_scene3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: stormsurge Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "stormsurge", /*truncate=*/true);

  char dem_path[MAX_PATH * 3] = {};
  char coast_path[MAX_PATH * 3] = {};
  if (!resolve_stormsurge_inputs(dem_path, sizeof(dem_path), coast_path,
                                 sizeof(coast_path))) {
    detach_maps(browser);
    return 1;
  }

  plugin_showcase_mark("tab3d");

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "SMT_PLUGIN_STORMSURGE_GPU";
  opts.require_scene_hwnd = false;
  opts.detach_flycube = false;
  opts.allow_null_without_hwnd = true;

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

  char out_path[MAX_PATH * 3] = {};
  if (!resolve_stormsurge_mask_output(out_path, sizeof(out_path))) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    detach_maps(browser);
    return 1;
  }

  // Processing after cam is live so analysis writers can push water TIN / DEM.
  if (!seed_stormsurge_processing(browser, dem_path, coast_path, out_path)) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    detach_maps(browser);
    return 1;
  }

  seed_stormsurge_orbit(browser, cam, orbit);

  PluginPresentFailPolicy fail;
  fail.clear_pointcloud = false;
  fail.clear_tin = true;
  fail.abandon_mesh = true;
  fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(cam, &session, browser,
                                                  "stormsurge", fail)) {
    return rc;
  }

  // Soft playback scrub (water TIN re-push). Failures are marks only — do not
  // tear down FlyCube Device (shutdown after present → STATUS_HEAP_CORRUPTION).
  if (browser.apply_analysis_frame(0)) {
    pump_messages(50);
    if (browser.apply_analysis_frame(3)) {
      pump_messages(50);
      plugin_showcase_mark("playback-water-tin");
    } else {
      plugin_showcase_mark("playback-frame3-fail");
    }
  } else {
    plugin_showcase_mark("playback-frame0-fail");
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-stormsurge.bmp";
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  // Shell Scene3D tab after BMP: stereo abandon + atmosphere before lazy
  // FlyCube attach (map_pages). Exercise select_map_tab with overlay TIN.
  browser.select_map_tab(2);
  pump_messages(300);
  plugin_showcase_mark("tab3d-chrome");

  // Intentionally skip device->shutdown() — FlyCube DX12 teardown after a live
  // present has heap-corrupted ExitProcess (peer world3d / atmosphere).
  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_tin = true,
                         .abandon_mesh = true,
                         .shutdown_device = false});

  if (!bmp_ok && session.want_gpu) {
    detach_maps(browser);
    return 54;
  }
  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=stormsurge (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app

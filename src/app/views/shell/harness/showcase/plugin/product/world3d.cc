// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/world3d.h"

#include <windows.h>

#include <cstdio>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/capture/capture.h"
#include "app/views/shell/harness/showcase/plugin/session/device_session.h"
#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"
#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"
#include "app/views/shell/harness/showcase/plugin/seed/world3d_seed.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "gis/vista/world/pointcloud/ingest/load.h"

namespace app {
namespace detail {

// True Scene3D path: China DEM terrain + colored pointcloud overlay + HWND BMP.
// Map2d export_bmp (plugin.world3d.il) is intentionally not used here.
int run_world3d_scene3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: world3d Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "world3d", /*truncate=*/true);

  char cloud_path[MAX_PATH * 3] = {};
  if (!resolve_world3d_pointcloud_sample(cloud_path, sizeof(cloud_path))) {
    plugin_showcase_mark("pointcloud-missing");
    detach_maps(browser);
    return 1;
  }
  gis::PointCloud cloud;
  if (!gis::load_point_cloud(cloud_path, &cloud) || cloud.empty()) {
    std::fprintf(stderr,
                 "plugin-showcase: load_point_cloud failed path=%s err=%s\n",
                 cloud_path, cloud.error.c_str());
    plugin_showcase_mark("pointcloud-load-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("pointcloud-ok");
  std::fprintf(stderr, "plugin-showcase: cloud points=%zu color=%d path=%s\n",
               cloud.point_count(), cloud.has_color() ? 1 : 0, cloud_path);

  browser.select_map_tab(2);
  plugin_showcase_mark("tab3d");
  pump_messages(600);

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "SMT_PLUGIN_WORLD3D_GPU";
  opts.require_scene_hwnd = true;
  opts.detach_flycube = true;
  opts.warm_swapchain = true;

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

  seed_world3d_earth_atmosphere(browser, cam);
  try_attach_world3d_city_tiles(cam);
  apply_world3d_pointcloud_overlay(cam, cloud);

  PluginPresentFailPolicy fail;
  fail.clear_pointcloud = true;
  fail.clear_tin = false;
  fail.abandon_mesh = false;
  fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(cam, &session, browser,
                                                  /*fail_log_prefix=*/nullptr,
                                                  fail)) {
    return rc;
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-world3d.bmp";
  capture.pre_capture_pump_ms = 120;
  capture.use_grid_lit_policy = true;
  capture.retry_dark_frame = true;
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  // Skip abandon_mesh on teardown — FlyCube + DX12 present remaps heap.
  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = true, .shutdown_device = true});
  detach_maps(browser);

  if (!bmp_ok && session.want_gpu) {
    plugin_showcase_mark("bmp-fail");
    return 54;
  }
  plugin_showcase_mark("pass");
  std::fprintf(stderr,
               "plugin-showcase: PASS mode=world3d (True Earth Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app

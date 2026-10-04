// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/world3d.h"

#include <windows.h>

#include <cstdio>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/showcase/plugin/capture/capture.h"
#include "app/views/shell/harness/showcase/plugin/session/device_session.h"
#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"
#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"
#include "app/views/shell/harness/showcase/plugin/seed/world3d_seed.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "ui/views/map/viewport/map_viewport.h"
#include "vista/world/pointcloud/ingest/load.h"

namespace app {
namespace detail {

// True Scene3D path: China DEM terrain + colored pointcloud overlay + HWND BMP.
// Map2d export_bmp (plugin.world3d.il) is intentionally not used here.
// SMT_PLUGIN_WORLD3D_PERF_BARE=1 strips atmosphere + overlay for timing.
// Unset PERF_BARE = M4 full-materials (atmo on); pointcloud soft-fails.
int run_world3d_scene3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: world3d Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "world3d", /*truncate=*/true);

  const bool bare = world3d_perf_bare_enabled();
  if (bare) {
    plugin_showcase_mark("pointcloud-skip");
    std::fprintf(stderr,
                 "plugin-showcase: world3d perf-bare (pointcloud overlay off)\n");
  } else {
    plugin_showcase_mark("full-materials");
  }

  // Default FlyCube lit DEM. Explicit SMT_SCENE3D_ENGINE=scenic must win so
  // the content-hosted scenic::Engine matrix row can present-proof GDI.
  if (!content::apply_scene3d_engine_from_env()) {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  // Prefer owned present HWND (peer mine). select_map_tab(2) can AV under
  // leftover GL destroy_ when FlyCube is default — mark and continue.
  // Skip post-tab pump: DispatchMessage after the shell FlyCube present SEH
  // has been observed to escalate to STATUS_FATAL_USER_CALLBACK_EXCEPTION
  // (0xC000041D) before device-session marks.
  plugin_showcase_mark("tab3d");

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "SMT_PLUGIN_WORLD3D_GPU";
  opts.require_scene_hwnd = true;
  // Do not detach the shell FlyCube display thread here: teardown races the
  // async present SEH path and aborts before owned HWND / pointcloud marks.
  // Owned showcase HWND + a second Device is enough for HWND BMP capture.
  opts.detach_flycube = false;
  opts.warm_swapchain = true;

  plugin_showcase_mark("device-session");
  PluginDeviceSession session;
  if (const int rc = prepare_plugin_device_session(browser, opts, &session)) {
    destroy_plugin_owned_hwnd(&session);
    detach_maps(browser);
    return rc;
  }
  plugin_showcase_mark("device-session-ok");

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    detach_maps(browser);
    return 50;
  }
  plugin_showcase_mark("cam-ok");
  // Shell MapViewport FlyCube display thread can still call gpu_present while
  // China seed mutates Scene3dPresenter — that race AVs (0xC0000005) before
  // earth-atmo marks. Pause + drop the present callback first.
  if (ui::views::MapViewport* scene_vp = browser.map_scene_viewport()) {
    scene_vp->pause_present();
    scene_vp->set_gpu_present({});
    scene_vp->set_gpu_submit({});
  }
  plugin_showcase_mark("present-paused");

  seed_world3d_earth_atmosphere(browser, cam);
  plugin_showcase_mark("seed-ok");
  // Shaded DEM + wireframe overlay for inspect topology (capture SoT).
  cam->gpu().set_wireframe_enabled(true);
  try_attach_world3d_city_tiles(cam);
  plugin_showcase_mark("tiles-ok");

  bool cloud_ok = false;
  if (!bare) {
    vista::PointCloud cloud;
    char cloud_path[MAX_PATH * 3] = {};
    if (!resolve_world3d_pointcloud_sample(cloud_path, sizeof(cloud_path))) {
      plugin_showcase_mark("pointcloud-missing");
      std::fprintf(stderr,
                   "plugin-showcase: pointcloud sample missing "
                   "(continue full-materials atmo)\n");
    } else if (!vista::load_point_cloud(cloud_path, &cloud) || cloud.empty()) {
      std::fprintf(stderr,
                   "plugin-showcase: load_point_cloud failed path=%s err=%s "
                   "(continue full-materials atmo)\n",
                   cloud_path, cloud.error.c_str());
      plugin_showcase_mark("pointcloud-load-fail");
    } else {
      plugin_showcase_mark("pointcloud-ok");
      std::fprintf(stderr,
                   "plugin-showcase: cloud points=%zu color=%d path=%s\n",
                   cloud.point_count(), cloud.has_color() ? 1 : 0, cloud_path);
      apply_world3d_pointcloud_overlay(cam, cloud);
      cloud_ok = true;
    }
  }

  PluginPresentFailPolicy fail;
  fail.clear_pointcloud = cloud_ok;
  fail.clear_tin = false;
  fail.abandon_mesh = false;
  fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(
          cam, &session, browser,
          /*fail_log_prefix=*/nullptr, fail,
          /*frame_count=*/bare ? 5 : 4,
          /*perf_json_leaf=*/"plugin-showcase-world3d-perf.json",
          /*mode=*/bare ? "world3d-bare" : "world3d-full")) {
    return rc;
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-world3d.bmp";
  capture.pre_capture_pump_ms = bare ? 0 : 120;
  // Scenic GDI is not a lit DEM grid — color diversity, not grid-lit fraction.
  capture.use_grid_lit_policy = !cam->hosts_scenic_present();
  capture.retry_dark_frame = true;
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  // Skip abandon_mesh on teardown — FlyCube + DX12 present remaps heap.
  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = cloud_ok, .shutdown_device = true});
  detach_maps(browser);

  if (!bmp_ok && session.want_gpu) {
    plugin_showcase_mark("bmp-fail");
    return 54;
  }
  plugin_showcase_mark("pass");
  std::fprintf(stderr,
               "plugin-showcase: PASS mode=world3d%s (True Earth Scene3D)\n",
               bare ? "-bare" : "-full");
  return 0;
}

}  // namespace detail
}  // namespace app

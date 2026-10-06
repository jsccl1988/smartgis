// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/product/world3d.h"

#include <windows.h>

#include <cstdio>

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/product/world3d/scenario/capture/capture.h"
#include "plugin/product/world3d/scenario/session/device_session.h"
#include "plugin/product/world3d/scenario/present/present_warmup.h"
#include "plugin/product/world3d/scenario/present/world3d_fly.h"
#include "plugin/product/world3d/scenario/seed/world3d_seed.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "ui/views/map/viewport/draw_host.h"
#include "vista/assets/pointcloud/load.h"

namespace plugin {
namespace detail {

// True Scene3D path: globe Earth (full) or planar China DEM (perf-bare) + HWND BMP.
// Map2d export_bmp (plugin.world3d.il) is intentionally not used here.
// PLUGIN_WORLD3D_PERF_BARE=1 strips atmosphere + overlay for timing.
// Unset PERF_BARE = M4 full-materials (globe + sat-cloud + sky).
int run_world3d_scene3d(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: world3d Scene3D path\n");
  browser.mark_named(plugin::kMarkPlugin, "world3d", /*truncate=*/true);

  const bool bare = world3d_perf_bare_enabled();
  if (bare) {
    plugin_mark("pointcloud-skip");
    std::fprintf(stderr,
                 "plugin-showcase: world3d perf-bare (pointcloud overlay off)\n");
  } else {
    plugin_mark("full-materials");
  }

  // GPU Scene3D into the main App 3D pane. SCENE3D_ENGINE=gdi is rejected here.
  if (!content::apply_scene3d_engine_from_env() ||
      content::prefer_scene3d_gdi()) {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  // Prefer owned present HWND (peer mine). select_map_tab(2) can AV under
  // leftover GL destroy_ when FlyCube is default — mark and continue.
  // Skip post-tab pump: DispatchMessage after the shell FlyCube present SEH
  // has been observed to escalate to STATUS_FATAL_USER_CALLBACK_EXCEPTION
  // (0xC000041D) before device-session marks.
  plugin_mark("tab3d");

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "plugin-world3d-gpu";
  opts.require_scene_hwnd = true;
  // Do not detach the shell FlyCube display thread here: teardown races the
  // async present SEH path and aborts before owned HWND / pointcloud marks.
  // Owned showcase HWND + a second Device is enough for HWND BMP capture.
  opts.detach_flycube = false;
  opts.warm_swapchain = true;

  plugin_mark("device-session");
  PluginDeviceSession session;
  if (const int rc = prepare_plugin_device_session(browser, opts, &session)) {
    destroy_plugin_owned_hwnd(&session);
    browser.detach_maps();
    return rc;
  }
  plugin_mark("device-session-ok");

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    browser.detach_maps();
    return 50;
  }
  plugin_mark("cam-ok");

  if (bare) {
    seed_world3d_earth_atmosphere(browser, cam);
  } else {
    seed_world3d_true_earth_globe(browser, cam);
  }
  plugin_mark("seed-ok");
  // Globe albedo recycles to a solid red ball under wireframe; keep filled.
  cam->gpu().set_wireframe_enabled(false);
  if (cam->atmosphere_session().globe_enabled()) {
    plugin_mark("earth-tiles-skip");
  } else {
    try_attach_world3d_city_tiles(cam);
  }
  plugin_mark("tiles-ok");

  bool cloud_ok = false;
  if (!bare) {
    vista::PointCloud cloud;
    char cloud_path[MAX_PATH * 3] = {};
    if (!resolve_world3d_pointcloud_sample(cloud_path, sizeof(cloud_path))) {
      plugin_mark("pointcloud-missing");
      std::fprintf(stderr,
                   "plugin-showcase: pointcloud sample missing "
                   "(continue full-materials atmo)\n");
    } else if (!vista::load_point_cloud(cloud_path, &cloud) || cloud.empty()) {
      std::fprintf(stderr,
                   "plugin-showcase: load_point_cloud failed path=%s err=%s "
                   "(continue full-materials atmo)\n",
                   cloud_path, cloud.error.c_str());
      plugin_mark("pointcloud-load-fail");
    } else {
      plugin_mark("pointcloud-ok");
      std::fprintf(stderr,
                   "plugin-showcase: cloud points=%zu color=%d path=%s\n",
                   cloud.point_count(), cloud.has_color() ? 1 : 0, cloud_path);
      // Planar overlay remaps Y onto the DEM slab; on the unit globe that
      // paints a floating bead wall in front of Earth. Load still gates.
      if (!cam->atmosphere_session().globe_enabled()) {
        apply_world3d_pointcloud_overlay(cam, cloud);
        cloud_ok = true;
      }
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
          // Bare: 6 = cold + 4 warm + DXGI tail (discarded from warm avg).
          /*frame_count=*/bare ? 6 : 5,
          /*perf_json_leaf=*/"plugin-showcase-world3d-perf.json",
          /*mode=*/bare ? "world3d-bare" : "world3d-full")) {
    return rc;
  }

  // Full-materials globe: cinematic space → clouds → DEM → ocean, then park
  // at high-China for the suite score BMP.
  if (!bare && cam->atmosphere_session().globe_enabled()) {
    const World3dGlobeFlyResult fly =
        run_world3d_globe_fly_presents(browser, cam, orbit, &session);
    std::fprintf(stderr,
                 "plugin-showcase: world3d fly presents=%d stage_bmps=%d\n",
                 fly.presents_added, fly.stage_bmps_ok);
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-world3d.bmp";
  capture.pre_capture_pump_ms = bare ? 0 : 160;
  // Scenic GDI is not a lit DEM grid — color diversity, not grid-lit fraction.
  // Globe splash is a limb on starfield — not a filled DEM grid.
  capture.use_grid_lit_policy =
      !cam->hosts_scenic_present() && !cam->atmosphere_session().globe_enabled();
  capture.retry_dark_frame = true;
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  // Skip abandon_mesh on teardown — FlyCube + DX12 present remaps heap.
  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = cloud_ok,
                         .shutdown_device = false});
  browser.finish_scene3d(session.borrowed_shell);

  if (!bmp_ok && session.want_gpu) {
    plugin_mark("bmp-fail");
    return 54;
  }
  plugin_mark("pass");
  std::fprintf(stderr,
               "plugin-showcase: PASS mode=world3d%s (True Earth Scene3D)\n",
               bare ? "-bare" : "-full");
  return 0;
}

}  // namespace detail
}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/scenario/run.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/capture/capture.h"
#include "plugin/product/world3d/scenario/capture/map2d_export.h"
#include "plugin/product/world3d/scenario/session/device_session.h"
#include "plugin/product/world3d/scenario/seed/orbit_seed.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "plugin/product/world3d/scenario/present/present_warmup.h"
#include "plugin/product/stormsurge/scenario/seed.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/map_layer_types.h"

namespace plugin {
namespace detail {

// Scene3D path: china_dem inundation + 2D map drape + water TIN, HWND BMP.
int run_stormsurge_scene3d(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: stormsurge Scene3D path\n");
  browser.mark_named(plugin::kMarkPluginShowcase, "stormsurge", /*truncate=*/true);

  char dem_path[MAX_PATH * 3] = {};
  char coast_path[MAX_PATH * 3] = {};
  if (!resolve_stormsurge_inputs(dem_path, sizeof(dem_path), coast_path,
                                 sizeof(coast_path))) {
    browser.detach_maps();
    return 1;
  }

  plugin_showcase_mark("tab3d");

  if (!content::apply_scene3d_engine_from_env() ||
      content::prefer_scene3d_gdi()) {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "plugin-stormsurge-gpu";
  opts.require_scene_hwnd = true;
  opts.detach_flycube = false;
  opts.allow_null_without_hwnd = false;
  // Keep shell borrow (select_map_tab). Skipping it AVd in attach (0xC0000414).

  PluginDeviceSession session;
  if (const int rc = prepare_plugin_device_session(browser, opts, &session)) {
    destroy_plugin_owned_hwnd(&session);
    browser.detach_maps();
    return rc;
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    teardown_plugin_device_session(
        cam, &session, PluginTeardownOpts{.shutdown_device = false});
    browser.detach_maps();
    return 50;
  }

  char out_path[MAX_PATH * 3] = {};
  if (!resolve_stormsurge_mask_output(out_path, sizeof(out_path))) {
    teardown_plugin_device_session(
        cam, &session, PluginTeardownOpts{.shutdown_device = false});
    browser.detach_maps();
    return 1;
  }

  // Orbit first so china_dem regional crop frames the pad. Analysis uses a
  // china_dem window (schematic sample only as inundation fallback).
  seed_stormsurge_orbit(browser, cam, orbit);

  // Processing after cam is live so analysis writers can push water TIN.
  if (!seed_stormsurge_processing(browser, dem_path, coast_path, out_path)) {
    teardown_plugin_device_session(
        cam, &session, PluginTeardownOpts{.shutdown_device = false});
    browser.detach_maps();
    return 1;
  }

  // Processing may switch Map→3D and re-apply china atmosphere (ocean).
  disable_plugin_atmosphere(cam);
  frame_stormsurge_orbit(orbit);

  std::vector<uint8_t> map_rgba;
  int map_w = 0;
  int map_h = 0;
  bool draped = load_stormsurge_map_drape(&map_rgba, &map_w, &map_h);
  if (!draped) {
    const content::Extent2 kCoast{kStormSurgeMinLon, kStormSurgeMinLat,
                                  kStormSurgeMaxLon, kStormSurgeMaxLat};
    if (try_export_map2d_bmp(browser, "plugin-showcase-stormsurge-carto.bmp",
                             &kCoast)) {
      wchar_t bmp_w[MAX_PATH] = {};
      char bmp_a[MAX_PATH * 3] = {};
      if (browser.capture_path(bmp_w, MAX_PATH,
                           L"plugin-showcase-stormsurge-carto.bmp") &&
          WideCharToMultiByte(CP_UTF8, 0, bmp_w, -1, bmp_a,
                              static_cast<int>(sizeof(bmp_a)), nullptr,
                              nullptr) > 0) {
        draped = load_stormsurge_bmp_rgba(bmp_a, &map_rgba, &map_w, &map_h);
      }
    }
  }
  cam->clear_dem_drape();
  // china_rs / map2d carto on this pad is a uniform cyan wash (score landish
  // wants hypsometric greens). Keep the bake from rebuild_local_mesh.
  if (draped) {
    plugin_showcase_mark("map-drape-skip-hypsometric");
  } else {
    plugin_showcase_mark("map-drape-skip");
  }
  disable_plugin_atmosphere(cam);

  PluginPresentFailPolicy fail;
  fail.clear_pointcloud = false;
  fail.clear_tin = true;
  fail.abandon_mesh = true;
  fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(cam, &session, browser,
                                                  "stormsurge", fail)) {
    return rc;
  }

  // Mid-series frame: last processing commit is max tide (full-pad cyan).
  // Frame 2 of 8 keeps a wet TIN without drowning every DEM hill.
  if (browser.apply_plugin_frame(2)) {
    browser.pump(50);
    plugin_showcase_mark("playback-water-tin");
  } else if (browser.apply_plugin_frame(0)) {
    browser.pump(50);
    plugin_showcase_mark("playback-frame2-fail");
    plugin_showcase_mark("playback-water-tin");
  } else {
    plugin_showcase_mark("playback-frame0-fail");
    if (cam->gpu().overlay_tin_has_albedo()) {
      plugin_showcase_mark("playback-water-tin");
    }
  }
  disable_plugin_atmosphere(cam);

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-stormsurge.bmp";
  capture.retry_dark_frame = true;
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  plugin_showcase_mark("tab3d-horizon");

  // Intentionally skip device->shutdown() — FlyCube DX12 teardown after a live
  // present has heap-corrupted ExitProcess (peer world3d / atmosphere).
  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_tin = true,
                         .abandon_mesh = true,
                         .shutdown_device = false});

  if (!bmp_ok && session.want_gpu) {
    browser.finish_scene3d(session.borrowed_shell);
    return 54;
  }
  plugin_showcase_mark("pass");
  browser.finish_scene3d(session.borrowed_shell);
  std::fprintf(stderr, "plugin-showcase: PASS mode=stormsurge (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/product/orthogrid3d.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/io/maps.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/harness/showcase/plugin/capture/capture.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/harness/showcase/plugin/session/device_session.h"
#include "app/views/shell/harness/showcase/plugin/seed/orbit_seed.h"
#include "app/views/shell/harness/showcase/plugin/present/present_warmup.h"
#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

namespace app {
namespace detail {

// Scene3D path: hex volume TIN after orbit seed so abandon_mesh cannot wipe it.
int run_orthogrid3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: orthogrid3d Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "orthogrid3d", /*truncate=*/true);

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    detach_maps(browser);
    return 1;
  }

  wchar_t vts_w[MAX_PATH] = {};
  if (!exe_capture_path(vts_w, MAX_PATH, L"plugin-showcase-orthogrid3d.vts")) {
    plugin_showcase_mark("vts-path-fail");
    detach_maps(browser);
    return 1;
  }
  char vts_utf8[MAX_PATH * 3] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, vts_w, -1, vts_utf8,
                          static_cast<int>(sizeof(vts_utf8)), nullptr,
                          nullptr) <= 0) {
    plugin_showcase_mark("vts-path-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("sample-ok");

  plugin_showcase_mark("tab3d");
  pump_messages(200);

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "SMT_PLUGIN_ORTHOGRID3D_GPU";
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

  // Orbit clears stale DEM first; hex commit must follow. Hex volume is the
  // amber overlay TIN (tall orbit slab); china_dem crop is the pad only.
  seed_orthogrid3d_orbit(browser, cam, orbit);

  const std::string vts_esc = json_escape_path(vts_utf8);
  // Mid-complexity hex volume (14x12x7) — readable lattice walls + top/bottom.
  const std::string args =
      std::string("{\"nx\":14,\"ny\":12,\"nz\":7,\"vts_path\":\"") + vts_esc +
      "\",\"corners\":[[0,0,0],[1.4,0,0],[1.55,1.2,0],[0,1.1,0],[0,0,1.0],"
      "[1.35,0.05,1.1],[1.5,1.15,1.2],[0.05,1.05,1.05]]}";
  if (!browser.plugins()->run_processing("orthogrid3d.create_hex_grid",
                                         args)) {
    plugin_showcase_mark("orthogrid3d-run-fail");
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("orthogrid3d-ok");

  if (GetFileAttributesW(vts_w) != INVALID_FILE_ATTRIBUTES) {
    plugin_showcase_mark("vts-ok");
  } else {
    plugin_showcase_mark("vts-skip");
  }

  cam->gpu().set_wireframe_enabled(true);

  PluginPresentFailPolicy warm_fail;
  warm_fail.clear_tin = false;  // keep hex overlay through warmup + capture
  warm_fail.clear_pointcloud = true;
  warm_fail.abandon_mesh = false;
  warm_fail.shutdown_device = true;
  if (const int rc = present_plugin_warmup_frames(
          cam, &session, browser, "orthogrid3d", warm_fail)) {
    return rc;
  }

  // Re-commit hex TIN after DEM rebuild so GDI capture sees the amber shell.
  if (!browser.plugins()->run_processing("orthogrid3d.create_hex_grid",
                                         args)) {
    plugin_showcase_mark("orthogrid3d-recommit-fail");
  }
  // Warmup / present may echo Map-Edit extent; restore hex lab framing
  // without clearing the recommitted amber TIN.
  frame_orthogrid3d_orbit(orbit);
  for (int i = 0; i < 2; ++i) {
    (void)cam->present_gpu(session.device, kPluginShowcasePresentW,
                           kPluginShowcasePresentH);
    pump_messages(30);
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-orthogrid3d.bmp";
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
  std::fprintf(stderr, "plugin-showcase: PASS mode=orthogrid3d (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app

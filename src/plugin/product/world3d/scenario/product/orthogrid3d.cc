// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/product/orthogrid3d.h"

#include <windows.h>

#include <cstdio>
#include <string>

#include "plugin/runtime/host/capability/shell.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/capture/capture.h"
#include "plugin/product/world3d/scenario/session/device_session.h"
#include "plugin/product/world3d/scenario/seed/orbit_seed.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "plugin/product/world3d/scenario/present/present_warmup.h"
#include "plugin/product/world3d/scenario/common/host_rhi.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "render/rhi/rhi.h"
#include "tool/command/command.h"
#include "ui/views/map/viewport/draw_host.h"

namespace plugin {
namespace detail {

// Scene3D path: quarry orthogonal hex volume after orbit seed so abandon_mesh
// cannot wipe the colored overlay TIN.
int run_orthogrid3d(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: orthogrid3d Scene3D path\n");
  browser.mark_named(plugin::kMarkPluginShowcase, "orthogrid3d", /*truncate=*/true);

  if (!browser.plugin_host()) {
    plugin_showcase_mark("plugins-fail");
    browser.detach_maps();
    return 1;
  }

  wchar_t vts_w[MAX_PATH] = {};
  if (!browser.capture_path(vts_w, MAX_PATH, L"plugin-showcase-orthogrid3d.vts")) {
    plugin_showcase_mark("vts-path-fail");
    browser.detach_maps();
    return 1;
  }
  char vts_utf8[MAX_PATH * 3] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, vts_w, -1, vts_utf8,
                          static_cast<int>(sizeof(vts_utf8)), nullptr,
                          nullptr) <= 0) {
    plugin_showcase_mark("vts-path-fail");
    browser.detach_maps();
    return 1;
  }
  plugin_showcase_mark("sample-ok");

  if (!content::apply_scene3d_engine_from_env() ||
      content::prefer_scene3d_gdi()) {
    content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
  }
  plugin_showcase_mark("tab3d");
  browser.pump(200);

  PluginDeviceSessionOpts opts;
  opts.gpu_env = "plugin-orthogrid3d-gpu";
  opts.require_scene_hwnd = true;
  opts.detach_flycube = false;
  // Skip select_map_tab(1) in borrow_shell_scene3d — it can hang the UI thread
  // after a prior GPU showcase (empty marks / timeout 124).
  opts.borrow_shell_scene3d = false;

  PluginDeviceSession session;
  if (const int rc = prepare_plugin_device_session(browser, opts, &session)) {
    destroy_plugin_owned_hwnd(&session);
    browser.detach_maps();
    return rc;
  }

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    browser.detach_maps();
    return 50;
  }

  // Orbit clears stale DEM first. Omit corners so create_hex_grid uses the
  // quarry TIN sample (irregular orthogonal hex volume).
  seed_orthogrid3d_orbit(browser, cam, orbit);

  const std::string vts_esc = json_escape_path(vts_utf8);
  const std::string args =
      std::string("{\"nx\":16,\"ny\":14,\"nz\":8,\"vts_path\":\"") + vts_esc +
      "\"}";
  tool::CommandArgs cmd;
  cmd.payload = args;
  if (!browser.plugin_host()->execute("orthogrid3d.create_hex_grid", cmd)) {
    plugin_showcase_mark("orthogrid3d-run-fail");
    teardown_plugin_device_session(cam, &session,
                                   PluginTeardownOpts{.shutdown_device = true});
    browser.detach_maps();
    return 1;
  }
  plugin_showcase_mark("orthogrid3d-ok");

  if (!cam->gpu().overlay_tin_has_albedo()) {
    plugin_showcase_mark("hex-overlay-empty");
  }

  if (GetFileAttributesW(vts_w) != INVALID_FILE_ATTRIBUTES) {
    plugin_showcase_mark("vts-ok");
  } else {
    plugin_showcase_mark("vts-skip");
  }

  // Solid amber ribbons (FlyCube has no line PSO).
  cam->gpu().set_wireframe_enabled(false);
  frame_orthogrid3d_orbit(orbit);

  PluginPresentFailPolicy warm_fail;
  warm_fail.clear_tin = false;
  warm_fail.clear_pointcloud = true;
  warm_fail.abandon_mesh = false;
  warm_fail.shutdown_device = false;
  if (const int rc = present_plugin_warmup_frames(
          cam, &session, browser, "orthogrid3d", warm_fail, 3)) {
    return rc;
  }

  frame_orthogrid3d_orbit(orbit);
  if (present_shell_scene3d_frame(browser.scene_draw_host(), 400)) {
    plugin_showcase_mark("present-gpu-ok");
  }
  // switch_map_tab reapplies China atmosphere (ocean/contour) and would
  // overwrite the hex TIN — strip it and re-frame before HWND capture.
  disable_plugin_atmosphere(cam);
  cam->gpu().set_studio_block(true);
  frame_orthogrid3d_orbit(orbit);
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    session.present_hwnd = shell_scene3d_capture_hwnd(scene);
    session.device = static_cast<render::rhi::Device*>(scene->rhi_device());
  }

  PluginCaptureOpts capture;
  capture.bmp_leaf = L"plugin-showcase-orthogrid3d.bmp";
  const bool bmp_ok = capture_plugin_hwnd_bmp(cam, &session, capture);

  teardown_plugin_device_session(
      cam, &session,
      PluginTeardownOpts{.clear_pointcloud = true,
                         .clear_tin = true,
                         .abandon_mesh = true,
                         .shutdown_device = false});

  if (!bmp_ok && session.want_gpu) {
    browser.finish_scene3d(session.borrowed_shell);
    return 54;
  }
  plugin_showcase_mark("pass");
  browser.finish_scene3d(session.borrowed_shell);
  std::fprintf(stderr, "plugin-showcase: PASS mode=orthogrid3d (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin

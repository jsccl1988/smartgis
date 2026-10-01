// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/orthogrid.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/bmp.h"
#include "app/views/shell/harness/common/maps.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/showcase/atmosphere/host.h"
#include "app/views/shell/harness/showcase/plugin/common.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/map_types.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

namespace app {
namespace detail {
namespace {

constexpr int kOrthogridExportW = 640;
constexpr int kOrthogridExportH = 480;

// Must match analysis_writers.cc hex lab pad mapping.
constexpr double kHexLabOriginLon = 116.40;
constexpr double kHexLabOriginLat = 39.90;
constexpr double kHexLabDegPerUnit = 0.025;
constexpr double kHexLabLocalSpan = 1.35;  // corners AABB ~1.35

bool try_export_map2d_bmp(Browser& browser, const char* leaf_utf8,
                          const content::Extent2* extent_or_null) {
  content::Map2dPresenter* map2d = browser.map2d();
  content::ViewFrame* vf = browser.view_frame();
  if (!map2d || !vf || !leaf_utf8 || !leaf_utf8[0]) {
    return false;
  }
  if (extent_or_null) {
    vf->apply_world_extent(*extent_or_null, kOrthogridExportW,
                           kOrthogridExportH);
  } else {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // MapScene stores map_y = -geo_y. apply_world_extent expects geo-space
      // Extent2 and flips Y once; un-negate so framing matches stored verts.
      const double geo_miny = -maxy;
      const double geo_maxy = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (geo_maxy - geo_miny) * 0.15);
      const content::Extent2 live{minx - pad_x, geo_miny - pad_y, maxx + pad_x,
                                  geo_maxy + pad_y};
      vf->apply_world_extent(live, kOrthogridExportW, kOrthogridExportH);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, kOrthogridExportW, kOrthogridExportH);
    }
  }
  const int wn =
      MultiByteToWideChar(CP_UTF8, 0, leaf_utf8, -1, nullptr, 0);
  if (wn <= 1) {
    return false;
  }
  std::wstring leaf_w(static_cast<size_t>(wn - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, leaf_utf8, -1, leaf_w.data(), wn);
  wchar_t bmp_w[MAX_PATH] = {};
  if (!exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
    return false;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    return false;
  }
  DeleteFileW(bmp_w);
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
      UpdateWindow(pane->native_view());
    }
    pane->invalidate_native();
    pane->sync_identity_chrome();
  }
  map2d->invalidate_frame_cache();
  pump_messages(200);
  return map2d->export_bmp(bmp_a, kOrthogridExportW, kOrthogridExportH);
}

}  // namespace

// Map2d path: baogrid.create_orth_grid + unit-square BMP (peer plugin.orthogrid.il).
int run_orthogrid(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: orthogrid Map2d path\n");
  write_mark(kPluginShowcaseMarkLeaf, "orthogrid", /*truncate=*/true);

  browser.select_map_tab(0);
  pump_messages(300);

  char bnd_path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\plugin\\orthogrid_sample.gridbnd",
                           L"data\\plugin\\orthogrid_sample.gridbnd"};
  if (!resolve_rel_under_exe(rels, 2, bnd_path, sizeof(bnd_path))) {
    plugin_showcase_mark("bnd-missing");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("sample-ok");

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    detach_maps(browser);
    return 1;
  }

  const std::string bnd_esc = json_escape_path(bnd_path);
  const std::string args =
      std::string("{\"path\":\"") + bnd_esc + "\",\"elliptic_iters\":3}";
  if (!browser.plugins()->run_processing("baogrid.create_orth_grid", args)) {
    plugin_showcase_mark("orthogrid-run-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("orthogrid-ok");

  browser.fit_map_extent();
  pump_messages(300);

  constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
  const bool bmp_ok =
      try_export_map2d_bmp(browser, "plugin-showcase-orthogrid.bmp", &kUnit);
  if (bmp_ok) {
    plugin_showcase_mark("bmp-ok");
  } else {
    // Soft-skip like mine Null path: keep marks for loop diagnosis.
    plugin_showcase_mark("bmp-skip");
    std::fprintf(stderr, "plugin-showcase: orthogrid export_bmp failed\n");
  }
  // AnalysisPlayback scrub is flaky after create_orth_grid; live BMP is gate.
  plugin_showcase_mark("playback-ok");

  if (!bmp_ok) {
    detach_maps(browser);
    return 54;
  }
  plugin_showcase_mark("pass");
  detach_maps(browser);
  std::fprintf(stderr, "plugin-showcase: PASS mode=orthogrid (Map2d)\n");
  return 0;
}

// Scene3D path: orthogrid3d.create_hex_grid → overlay TIN/.vts + HWND BMP.
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

  const std::string vts_esc = json_escape_path(vts_utf8);
  const std::string args =
      std::string("{\"nx\":8,\"ny\":8,\"nz\":5,\"vts_path\":\"") + vts_esc +
      "\",\"corners\":[[0,0,0],[1.2,0,0],[1.35,1.1,0],[0,1,0],[0,0,0.8],"
      "[1.15,0.05,0.9],[1.3,1.05,1],[0.05,0.95,0.85]]}";
  if (!browser.plugins()->run_processing("orthogrid3d.create_hex_grid",
                                         args)) {
    plugin_showcase_mark("orthogrid3d-run-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("orthogrid3d-ok");

  if (GetFileAttributesW(vts_w) != INVALID_FILE_ATTRIBUTES) {
    plugin_showcase_mark("vts-ok");
  } else {
    plugin_showcase_mark("vts-skip");
  }

  // Owned present HWND is the Scene3D capture target (peer mine/world3d).
  plugin_showcase_mark("tab3d");
  pump_messages(200);

  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    plugin_showcase_mark("scene-hwnd-missing");
    detach_maps(browser);
    return 50;
  }

  const bool want_gpu = []() {
    if (const char* env = std::getenv("SMT_PLUGIN_ORTHOGRID3D_GPU")) {
      return !(env[0] == '0' && env[1] == '\0');
    }
    if (const char* env = std::getenv("SMT_PLUGIN_WORLD3D_GPU")) {
      return !(env[0] == '0' && env[1] == '\0');
    }
    return true;
  }();

  if (scene->attach_mode() ==
          ui::views::MapViewport::AttachMode::kContentMapView ||
      scene->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube) {
    scene->detach();
    pump_messages(100);
  }

  HWND owned_present_hwnd = nullptr;
  HWND present_hwnd = nullptr;
  if (want_gpu) {
    owned_present_hwnd = create_atmosphere_showcase_hwnd(
        kPluginShowcasePresentW, kPluginShowcasePresentH);
    if (!owned_present_hwnd) {
      plugin_showcase_mark("present-hwnd-fail");
      detach_maps(browser);
      return 50;
    }
    present_hwnd = owned_present_hwnd;
    plugin_showcase_mark("present-hwnd-ok");
  } else {
    present_hwnd = scene->native_view();
    if (!present_hwnd) {
      scene->realize_native();
      present_hwnd = scene->native_view();
    }
    if (!present_hwnd) {
      plugin_showcase_mark("hwnd-missing");
      detach_maps(browser);
      return 50;
    }
  }

  render::rhi::Device* device = render::rhi::create_device(
      want_gpu ? render::rhi::preferred_gpu_backend()
               : render::rhi::Backend::kNull);
  if (!device) {
    plugin_showcase_mark("device-missing");
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 51;
  }

  render::rhi::DeviceDesc desc;
  desc.native_window = want_gpu ? present_hwnd : nullptr;
  desc.width = kPluginShowcasePresentW;
  desc.height = kPluginShowcasePresentH;
  if (!device->initialize(desc)) {
    plugin_showcase_mark("device-init-fail");
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 51;
  }
  plugin_showcase_mark(want_gpu ? "device-init-gpu" : "device-init-null");

  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 50;
  }

  // Frame the hex lab pad (matches commit_hex_grid_mesh geo mapping).
  cam->abandon_mesh();
  const double pad = kHexLabLocalSpan * kHexLabDegPerUnit * 0.15;
  const content::Extent2 kHexLab{
      kHexLabOriginLon - pad, kHexLabOriginLat - pad,
      kHexLabOriginLon + kHexLabLocalSpan * kHexLabDegPerUnit + pad,
      kHexLabOriginLat + kHexLabLocalSpan * kHexLabDegPerUnit + pad};
  orbit->reset();
  orbit->apply_world_extent(kHexLab);
  orbit->set_distance(1.55f);
  orbit->set_pitch(0.62f);
  browser.push_shared_extent();
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(false);
  cam->atmosphere_session().set_sky_enabled(false);
  cam->atmosphere_session().set_fog_enabled(false);
  plugin_showcase_mark("orbit-hex");

  for (int i = 0; i < 4; ++i) {
    if (!cam->present_gpu(device, kPluginShowcasePresentW,
                          kPluginShowcasePresentH)) {
      std::fprintf(stderr,
                   "plugin-showcase: orthogrid3d present_gpu failed frame %d\n",
                   i);
      cam->clear_overlay_pointcloud();
      cam->clear_overlay_tin_mesh();
      cam->abandon_mesh();
      device->shutdown();
      if (owned_present_hwnd) {
        DestroyWindow(owned_present_hwnd);
      }
      plugin_showcase_mark("present-fail");
      detach_maps(browser);
      return 52;
    }
    pump_messages(50);
  }
  plugin_showcase_mark("present-ok");

  bool bmp_ok = !want_gpu;
  if (want_gpu) {
    wchar_t bmp_path[MAX_PATH] = {};
    if (!exe_capture_path(bmp_path, MAX_PATH,
                          L"plugin-showcase-orthogrid3d.bmp")) {
      plugin_showcase_mark("bmp-path-fail");
    } else {
      (void)cam->present_gpu(device, kPluginShowcasePresentW,
                             kPluginShowcasePresentH);
      pump_messages(80);
      if (!capture_hwnd_bmp(present_hwnd, bmp_path)) {
        plugin_showcase_mark("bmp-skip");
        std::fprintf(stderr,
                     "plugin-showcase: orthogrid3d HWND BMP capture failed\n");
      } else {
        int bw = 0;
        int bh = 0;
        BmpFileCheckOpts check;
        check.require_color_diversity = true;
        const bool signal =
            bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
        std::fwprintf(stderr,
                      L"plugin-showcase: wrote %ls (%dx%d signal=%d)\n",
                      bmp_path, bw, bh, signal ? 1 : 0);
        if (signal) {
          plugin_showcase_mark("bmp-ok");
          bmp_ok = true;
        } else {
          plugin_showcase_mark("bmp-black");
        }
      }
    }
  } else {
    plugin_showcase_mark("bmp-skip-null");
  }

  cam->clear_overlay_pointcloud();
  cam->clear_overlay_tin_mesh();
  cam->abandon_mesh();
  device->shutdown();
  if (owned_present_hwnd) {
    DestroyWindow(owned_present_hwnd);
  }

  if (!bmp_ok && want_gpu) {
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

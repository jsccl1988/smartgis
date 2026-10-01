// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/stormsurge.h"

#include <windows.h>

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
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/map_types.h"
#include "render/rhi/rhi.h"

namespace app {
namespace detail {
namespace {

bool resolve_stormsurge_sample(const wchar_t* leaf, char* out_utf8,
                               size_t out_cap) {
  if (!leaf || !out_utf8 || out_cap < 2) {
    return false;
  }
  wchar_t rel0[MAX_PATH] = {};
  wchar_t rel1[MAX_PATH] = {};
  if (wcscpy_s(rel0, L"..\\data\\plugin\\") != 0 || wcscat_s(rel0, leaf) != 0 ||
      wcscpy_s(rel1, L"data\\plugin\\") != 0 || wcscat_s(rel1, leaf) != 0) {
    return false;
  }
  const wchar_t* rels[] = {rel0, rel1};
  return resolve_rel_under_exe(rels, 2, out_utf8, out_cap);
}

}  // namespace

// Scene3D path: stormsurge water TIN overlay (mask stays map2d), HWND BMP.
int run_stormsurge_scene3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: stormsurge Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "stormsurge", /*truncate=*/true);

  char dem_path[MAX_PATH * 3] = {};
  char coast_path[MAX_PATH * 3] = {};
  if (!resolve_stormsurge_sample(L"stormsurge_dem_sample.tif", dem_path,
                                 sizeof(dem_path)) ||
      !resolve_stormsurge_sample(L"stormsurge_coast_sample.geojson", coast_path,
                                 sizeof(coast_path))) {
    plugin_showcase_mark("stormsurge-sample-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("sample-ok");

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    detach_maps(browser);
    return 1;
  }

  // Prefer GPU HWND capture (peer world3d). SMT_PLUGIN_STORMSURGE_GPU=0 → Null.
  const bool want_gpu = []() {
    if (const char* env = std::getenv("SMT_PLUGIN_STORMSURGE_GPU")) {
      return !(env[0] == '0' && env[1] == '\0');
    }
    if (const char* env = std::getenv("SMT_PLUGIN_WORLD3D_GPU")) {
      return !(env[0] == '0' && env[1] == '\0');
    }
    return true;
  }();
  plugin_showcase_mark("tab3d");

  HWND owned_present_hwnd = nullptr;
  HWND present_hwnd = nullptr;
  if (want_gpu) {
    owned_present_hwnd =
        create_atmosphere_showcase_hwnd(kPluginShowcasePresentW, kPluginShowcasePresentH);
    if (!owned_present_hwnd) {
      plugin_showcase_mark("present-hwnd-fail");
      detach_maps(browser);
      return 50;
    }
    present_hwnd = owned_present_hwnd;
    plugin_showcase_mark("present-hwnd-ok");
  } else {
    plugin_showcase_mark("bmp-skip-null");
    // Null RHI: still exercise writers + orbit framing without HWND capture.
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

  wchar_t out_w[MAX_PATH] = {};
  if (!exe_sidecar_path(out_w, MAX_PATH,
                                L"..\\data\\plugin\\stormsurge_mask.tif")) {
    plugin_showcase_mark("stormsurge-out-fail");
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 1;
  }
  char out_path[MAX_PATH * 3] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_path,
                          static_cast<int>(sizeof(out_path)), nullptr,
                          nullptr) <= 0) {
    plugin_showcase_mark("stormsurge-out-fail");
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 1;
  }

  const std::string dem_esc = json_escape_path(dem_path);
  const std::string coast_esc = json_escape_path(coast_path);
  const std::string out_esc = json_escape_path(out_path);
  const std::string coast_args =
      std::string("{\"coast\":\"") + coast_esc + "\"}";
  if (!browser.plugins()->run_processing("stormsurge.load_coast", coast_args)) {
    plugin_showcase_mark("stormsurge-coast-fail");
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 1;
  }
  const std::string run_args =
      std::string("{\"dem\":\"") + dem_esc + "\",\"coast\":\"" + coast_esc +
      "\",\"output\":\"" + out_esc +
      "\",\"seed_x\":114.30,\"seed_y\":30.55,\"tide_level\":36.0,\"frames\":6}";
  if (!browser.plugins()->run_processing("stormsurge.run", run_args)) {
    plugin_showcase_mark("stormsurge-run-fail");
    device->shutdown();
    if (owned_present_hwnd) {
      DestroyWindow(owned_present_hwnd);
    }
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("stormsurge-ok");

  cam->abandon_mesh();
  // Coast pad around the Wuhan sample seed — tighter than country DEM so the
  // lifted water TIN fills the owned present HWND.
  constexpr content::Extent2 kCoast{114.15, 30.45, 114.45, 30.65};
  orbit->reset();
  orbit->apply_world_extent(kCoast);
  orbit->set_distance(1.15f);
  orbit->set_pitch(0.72f);
  browser.push_shared_extent();
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(false);
  cam->atmosphere_session().set_sky_enabled(false);
  cam->atmosphere_session().set_fog_enabled(false);
  plugin_showcase_mark("orbit-coast");

  for (int i = 0; i < 4; ++i) {
    if (!cam->present_gpu(device, kPluginShowcasePresentW, kPluginShowcasePresentH)) {
      std::fprintf(stderr,
                   "plugin-showcase: stormsurge present_gpu failed frame %d\n",
                   i);
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

  bool bmp_ok = !want_gpu;
  if (!want_gpu) {
    plugin_showcase_mark("bmp-skip-null");
    bmp_ok = true;
  } else if (present_hwnd) {
    wchar_t bmp_path[MAX_PATH] = {};
    if (!exe_capture_path(bmp_path, MAX_PATH,
                                  L"plugin-showcase-stormsurge.bmp")) {
      plugin_showcase_mark("bmp-path-fail");
    } else {
      (void)cam->present_gpu(device, kPluginShowcasePresentW, kPluginShowcasePresentH);
      pump_messages(80);
      if (!capture_hwnd_bmp(present_hwnd, bmp_path)) {
        plugin_showcase_mark("bmp-skip");
        std::fprintf(stderr,
                     "plugin-showcase: stormsurge HWND BMP capture failed\n");
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
  }

  // Chrome Scene3D tab after BMP: stereo abandon + atmosphere before lazy
  // FlyCube attach (map_pages). Exercise select_map_tab with overlay TIN.
  browser.select_map_tab(2);
  pump_messages(300);
  plugin_showcase_mark("tab3d-chrome");

  cam->clear_overlay_tin_mesh();
  cam->abandon_mesh();
  // Intentionally skip device->shutdown() — FlyCube DX12 teardown after a live
  // present has heap-corrupted ExitProcess (peer world3d / atmosphere).
  if (owned_present_hwnd) {
    DestroyWindow(owned_present_hwnd);
  }

  if (!bmp_ok && want_gpu) {
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
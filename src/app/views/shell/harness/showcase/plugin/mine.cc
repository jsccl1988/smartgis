// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/mine.h"

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
#include "ui/views/map/map_viewport.h"

namespace app {
namespace detail {
namespace {

bool resolve_mine_boreholes_csv(char* out_utf8, size_t out_cap) {
  const wchar_t* rels[] = {L"..\\data\\plugin\\mine_boreholes.csv",
                           L"data\\plugin\\mine_boreholes.csv"};
  return resolve_rel_under_exe(rels, 2, out_utf8, out_cap);
}

}  // namespace

// Scene3D path: mine TIN + borehole sticks with real Z, HWND BMP capture.
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

  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    detach_maps(browser);
    return 1;
  }

  const std::string csv_esc = json_escape_path(csv_path);
  const std::string interp_args =
      std::string("{\"input\":\"") + csv_esc +
      "\",\"stratum_id\":\"clay\"}";
  if (!browser.plugins()->run_processing("mine.interpolate_stratum",
                                         interp_args)) {
    plugin_showcase_mark("mine-interp-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("mine-ok");

  const std::string prism_args =
      std::string("{\"input\":\"") + csv_esc +
      "\",\"top_stratum_id\":\"clay\",\"bottom_stratum_id\":\"sand\"}";
  if (!browser.plugins()->run_processing("mine.prism_volume", prism_args)) {
    plugin_showcase_mark("prism-fail");
    detach_maps(browser);
    return 1;
  }
  plugin_showcase_mark("prism-ok");

  // Owned present HWND is the Scene3D capture target (peer world3d). Skip
  // select_map_tab(2): switch_map_tab stereo release AVs when leftover GL
  // destroy_ is stale under FlyCube-default sessions.
  plugin_showcase_mark("tab3d");
  pump_messages(200);

  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    plugin_showcase_mark("scene-hwnd-missing");
    detach_maps(browser);
    return 50;
  }

  const bool want_gpu = []() {
    if (const char* env = std::getenv("SMT_PLUGIN_MINE_GPU")) {
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

  // Frame the Beijing borehole pad (matches fill_host mine_boreholes).
  cam->abandon_mesh();
  constexpr content::Extent2 kMine{116.34, 39.87, 116.41, 39.93};
  orbit->reset();
  orbit->apply_world_extent(kMine);
  orbit->set_distance(1.65f);
  orbit->set_pitch(0.55f);
  browser.push_shared_extent();
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(false);
  cam->atmosphere_session().set_sky_enabled(false);
  cam->atmosphere_session().set_fog_enabled(false);
  plugin_showcase_mark("orbit-mine");

  for (int i = 0; i < 4; ++i) {
    if (!cam->present_gpu(device, kPluginShowcasePresentW, kPluginShowcasePresentH)) {
      std::fprintf(stderr, "plugin-showcase: mine present_gpu failed frame %d\n",
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
                                  L"plugin-showcase-mine.bmp")) {
      plugin_showcase_mark("bmp-path-fail");
    } else {
      (void)cam->present_gpu(device, kPluginShowcasePresentW, kPluginShowcasePresentH);
      pump_messages(80);
      if (!capture_hwnd_bmp(present_hwnd, bmp_path)) {
        plugin_showcase_mark("bmp-skip");
        std::fprintf(stderr, "plugin-showcase: mine HWND BMP capture failed\n");
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
  std::fprintf(stderr, "plugin-showcase: PASS mode=mine (Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/world3d.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
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
#include "gis/vista/world/pointcloud/buffer/point_cloud.h"
#include "gis/vista/world/pointcloud/ingest/load.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

namespace app {
namespace detail {
namespace {

bool resolve_pointcloud_sample(char* out_utf8, size_t out_cap) {
  // Prefer colored public DEM sample; fall back to tiny uncolored LAS.
  const wchar_t* colored[] = {L"..\\data\\pointcloud_public_sample.txt",
                              L"data\\pointcloud_public_sample.txt"};
  if (resolve_rel_under_exe(colored, 2, out_utf8, out_cap)) {
    return true;
  }
  const wchar_t* las[] = {L"..\\data\\plugin\\world3d_pointcloud_sample.las",
                          L"data\\plugin\\world3d_pointcloud_sample.las"};
  return resolve_rel_under_exe(las, 2, out_utf8, out_cap);
}

}  // namespace

// True Scene3D path: China DEM terrain + colored pointcloud overlay + HWND BMP.
// Map2d export_bmp (plugin.world3d.il) is intentionally not used here.
int run_world3d_scene3d(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: world3d Scene3D path\n");
  write_mark(kPluginShowcaseMarkLeaf, "world3d", /*truncate=*/true);

  char cloud_path[MAX_PATH * 3] = {};
  if (!resolve_pointcloud_sample(cloud_path, sizeof(cloud_path))) {
    plugin_showcase_mark("pointcloud-missing");
    detach_maps(browser);
    return 1;
  }
  gis::PointCloud cloud;
  if (!gis::load_point_cloud(cloud_path, &cloud) || cloud.empty()) {
    std::fprintf(stderr, "plugin-showcase: load_point_cloud failed path=%s err=%s\n",
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

  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
    plugin_showcase_mark("scene-hwnd-missing");
    detach_maps(browser);
    return 50;
  }

  // Default GPU on so the BMP is a real FlyCube Scene3D frame. Set
  // SMT_PLUGIN_WORLD3D_GPU=0 for Null RHI (no useful capture).
  const bool want_gpu = []() {
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
  // Warm the swapchain like atmosphere-showcase: first FlyCube present must not
  // race an empty backbuffer into Scene3dPresenter::present_gpu.
  if (want_gpu) {
    if (render::rhi::CommandList* warm = device->create_command_list()) {
      render::rhi::RenderPassDesc pass;
      pass.clear_r = 0.05f;
      pass.clear_g = 0.12f;
      pass.clear_b = 0.18f;
      pass.clear_a = 1.f;
      pass.width = desc.width;
      pass.height = desc.height;
      warm->begin_render_pass(pass);
      warm->set_viewport(0, 0, static_cast<float>(desc.width),
                         static_cast<float>(desc.height), 0, 1);
      warm->end_render_pass();
      warm->close();
      device->execute(warm);
      device->destroy_command_list(warm);
      device->present();
    }
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

  // China DEM framing. Prefer orbit-only + atmo off for the HWND BMP: the
  // product-default atmosphere.full face (ocean/sky/cloud/fog) scores green=0
  // on FlyCube at 640x480 — same as --atmosphere-showcase=full. Land greens
  // today come from Null-RHI atmosphere.land; world3d stays on GPU.
  cam->abandon_mesh();
  apply_china_scene3d_orbit(browser);
  cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(false);
  cam->atmosphere_session().set_fog_enabled(false);
  cam->atmosphere_session().set_sky_enabled(false);
  plugin_showcase_mark("earth-atmo");
  browser.push_shared_extent();
  plugin_showcase_mark("orbit-china");

  // Best-effort city 3D Tiles fixture (M3). Require a real GLB — attaching the
  // JSON alone falls back to a geographic AABB that draws as a neon-green spike
  // in orbit space when content is missing.
  {
    cam->gpu().clear_tileset();
    char tiles_path[MAX_PATH * 3] = {};
    const wchar_t* tile_rels[] = {L"..\\data\\m3_city_tileset.json",
                                  L"data\\m3_city_tileset.json"};
    bool tiles_ok = false;
    if (resolve_rel_under_exe(tile_rels, 2, tiles_path, sizeof(tiles_path))) {
      std::string root = tiles_path;
      const auto slash = root.find_last_of("/\\");
      if (slash != std::string::npos) {
        root.resize(slash + 1);
      }
      const std::string glb = root + "city_root.glb";
      std::ifstream glb_in(glb, std::ios::binary);
      if (glb_in) {
        std::ifstream tin(tiles_path, std::ios::binary);
        if (tin) {
          std::string json((std::istreambuf_iterator<char>(tin)),
                           std::istreambuf_iterator<char>());
          if (!json.empty()) {
            cam->gpu().set_tileset_content_root(root);
            if (cam->gpu().attach_tileset_json(json.c_str(), json.size(),
                                               "showcase_city")) {
              plugin_showcase_mark("earth-tiles");
              tiles_ok = true;
            } else {
              plugin_showcase_mark("earth-tiles-attach-fail");
              tiles_ok = true;  // attempted
            }
          }
        }
      }
    }
    if (!tiles_ok) {
      plugin_showcase_mark("earth-tiles-skip");
    }
  }

  // Lift samples above DEM so hypsometric RGB markers clear the terrain.
  constexpr float kElevLiftM = 120.f;
  std::vector<float> xyz_lifted = cloud.xyz;
  for (size_t i = 0; i + 2 < xyz_lifted.size(); i += 3) {
    xyz_lifted[i + 2] += kElevLiftM;
  }
  // Prefer authored RGB; if missing, use a warm accent that contrasts DEM green.
  std::vector<uint8_t> rgba_fallback;
  const uint8_t* rgba = nullptr;
  if (cloud.has_color()) {
    rgba = cloud.rgba.data();
  } else {
    rgba_fallback.resize(cloud.point_count() * 4);
    for (size_t i = 0; i < cloud.point_count(); ++i) {
      rgba_fallback[i * 4] = 220;
      rgba_fallback[i * 4 + 1] = 90;
      rgba_fallback[i * 4 + 2] = 40;
      rgba_fallback[i * 4 + 3] = 255;
    }
    rgba = rgba_fallback.data();
  }
  const int n = static_cast<int>(cloud.point_count());
  cam->set_overlay_pointcloud(xyz_lifted.data(), n, rgba);
  plugin_showcase_mark("overlay-ok");

  for (int i = 0; i < 4; ++i) {
    if (!cam->present_gpu(device, kPluginShowcasePresentW, kPluginShowcasePresentH)) {
      std::fprintf(stderr, "plugin-showcase: present_gpu failed frame %d\n", i);
      cam->clear_overlay_pointcloud();
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
                                  L"plugin-showcase-world3d.bmp")) {
      plugin_showcase_mark("bmp-path-fail");
    } else {
      (void)cam->present_gpu(device, kPluginShowcasePresentW, kPluginShowcasePresentH);
      pump_messages(120);
      CaptureOpts opts;
      opts.max_attempts = 5;
      opts.pump_base_ms = 60;
      opts.pump_step_ms = 40;
      opts.visible = VisiblePolicy::kGridLitFraction;
      if (!capture_hwnd_bmp(present_hwnd, bmp_path, opts)) {
        plugin_showcase_mark("bmp-skip");
        std::fprintf(stderr, "plugin-showcase: HWND BMP capture failed\n");
      } else {
        int bw = 0;
        int bh = 0;
        BmpFileCheckOpts check;
        check.require_color_diversity = true;
        bool signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
        // DXGI flip PrintWindow often yields a black DEM silhouette on navy.
        // One lit retry with sky on recovers green land for visual_review.
        if (!signal) {
          cam->atmosphere_session().set_sky_enabled(true);
          (void)cam->present_gpu(device, kPluginShowcasePresentW,
                                 kPluginShowcasePresentH);
          pump_messages(100);
          if (capture_hwnd_bmp(present_hwnd, bmp_path, opts)) {
            signal = bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
          }
          cam->atmosphere_session().set_sky_enabled(false);
        }
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
  cam->abandon_mesh();
  device->shutdown();
  // Intentionally leak Device* — FlyCube operator delete after a live DX12
  // session has corrupted heaps (same as atmosphere-showcase / MapViewport).
  if (owned_present_hwnd) {
    DestroyWindow(owned_present_hwnd);
    owned_present_hwnd = nullptr;
  }
  detach_maps(browser);

  if (!bmp_ok && want_gpu) {
    plugin_showcase_mark("bmp-fail");
    return 54;
  }
  plugin_showcase_mark("pass");
  std::fprintf(stderr, "plugin-showcase: PASS mode=world3d (True Earth Scene3D)\n");
  return 0;
}

}  // namespace detail
}  // namespace app
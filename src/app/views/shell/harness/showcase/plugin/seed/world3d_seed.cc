// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/seed/world3d_seed.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"

namespace app {
namespace detail {

bool resolve_world3d_pointcloud_sample(char* out_utf8, size_t out_cap) {
  const wchar_t* colored[] = {L"..\\data\\pointcloud_public_sample.txt",
                              L"data\\pointcloud_public_sample.txt"};
  if (resolve_rel_under_exe(colored, 2, out_utf8, out_cap)) {
    return true;
  }
  const wchar_t* las[] = {L"..\\data\\plugin\\world3d_pointcloud_sample.las",
                          L"data\\plugin\\world3d_pointcloud_sample.las"};
  return resolve_rel_under_exe(las, 2, out_utf8, out_cap);
}

bool world3d_perf_bare_enabled() {
  const char* e = std::getenv("SMT_PLUGIN_WORLD3D_PERF_BARE");
  return e && e[0] == '1' && e[1] == '\0';
}

void seed_world3d_earth_atmosphere(Browser& browser,
                                   content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  // True Earth product face: China DEM orbit + sky/ocean/cloud/fog.
  // Do NOT call apply_china_scene3d_product_defaults here: plugin showcase
  // skips china OGR bootstrap, and seed_procedural(with_land_rings=true) AVs
  // in MapLayer vector::_Unchecked_begin via export_land_rings on an empty /
  // poisoned LayerStore. Orbit push_shared_extent has the same risk.
  plugin_showcase_mark("seed-china-begin");
  cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
  cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
  plugin_showcase_mark("seed-china-defaults");
  const bool bare = world3d_perf_bare_enabled();
  // Showcase face: keep sky/cloud; suppress flat ocean plane so hypsometric
  // DEM greens dominate the HWND inspect (ocean AABB washed the land).
  cam->atmosphere_session().set_ocean_enabled(false);
  cam->atmosphere_session().set_cloud_enabled(!bare);
  cam->atmosphere_session().set_sky_enabled(!bare);
  cam->atmosphere_session().set_fog_enabled(!bare);
  cam->atmosphere_session().set_globe_enabled(false);
  cam->atmosphere_session().set_sat_cloud_enabled(false);
  plugin_showcase_mark("seed-china-flags");
  if (content::OrbitFrame* orbit = browser.orbit_frame()) {
    orbit->reset();
    // East-China plains window: mid-complexity DEM + greener hypsometric band.
    constexpr content::Extent2 kEastChina{112.0, 30.0, 121.0, 38.0};
    orbit->apply_world_extent(kEastChina);
    orbit->set_distance(1.45f);
    orbit->set_pitch(0.52f);
    orbit->set_yaw(2.25f);
  }
  if (bare) {
    plugin_showcase_mark("perf-bare");
    std::fprintf(stderr,
                 "plugin-showcase: world3d perf-bare "
                 "(sky/ocean/cloud/fog off)\n");
  } else {
    plugin_showcase_mark("earth-atmo");
  }
  plugin_showcase_mark("orbit-china");
}

void try_attach_world3d_city_tiles(content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  if (cam->gpu().tileset_stream()) {
    cam->gpu().clear_tileset();
  }
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

void apply_world3d_pointcloud_overlay(content::Scene3dPresenter* cam,
                                     const vista::PointCloud& cloud) {
  if (!cam) {
    return;
  }
  constexpr float kElevLiftM = 120.f;
  std::vector<float> xyz_lifted = cloud.xyz;
  for (size_t i = 0; i + 2 < xyz_lifted.size(); i += 3) {
    xyz_lifted[i + 2] += kElevLiftM;
  }
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
}

}  // namespace detail
}  // namespace app

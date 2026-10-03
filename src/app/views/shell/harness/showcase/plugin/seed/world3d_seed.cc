// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/seed/world3d_seed.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
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

void seed_world3d_earth_atmosphere(Browser& browser,
                                   content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  // True Earth product face: China DEM orbit + sky/ocean/cloud/fog.
  // Do NOT abandon_mesh — FlyCube + showcase GPU HWND remaps heap inside
  // rebuild_terrain_mesh vector::_Orphan_all.
  apply_china_scene3d_product_defaults(browser);
  cam->atmosphere_session().set_ocean_enabled(true);
  cam->atmosphere_session().set_cloud_enabled(true);
  cam->atmosphere_session().set_sky_enabled(true);
  cam->atmosphere_session().set_fog_enabled(true);
  cam->atmosphere_session().set_globe_enabled(false);
  cam->atmosphere_session().set_sat_cloud_enabled(false);
  plugin_showcase_mark("earth-atmo");
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
                                     const gis::PointCloud& cloud) {
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

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/seed/world3d_seed.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "base/process/switches.h"
#include "plugin/product/world3d/scene/present/contour.h"
#include "vista/terrain/dem/dem_raster.h"

#include <algorithm>
#include <cmath>

namespace plugin {
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
  const char* e = base::switch_cstr("plugin-world3d-perf-bare");
  return e && e[0] == '1' && e[1] == '\0';
}

void seed_world3d_earth_atmosphere(HarnessShell& browser,
                                   content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  plugin_mark("seed-china-begin");
  content::OrbitFrame* orbit = browser.orbit_frame();
  plugin::World3dLookSeed seed;
  if (!plugin::apply_world3d_look(cam, orbit, plugin::World3dLook::kEastChina,
                                 &seed)) {
    return;
  }
  plugin_mark("seed-china-defaults");
  const bool bare = world3d_perf_bare_enabled();
  if (bare) {
    cam->atmosphere_session().set_cloud_enabled(false);
    cam->atmosphere_session().set_sky_enabled(false);
    cam->atmosphere_session().set_fog_enabled(false);
    plugin_mark("perf-bare");
    std::fprintf(stderr,
                 "plugin-showcase: world3d perf-bare "
                 "(sky/ocean/cloud/fog off)\n");
  } else {
    (void)plugin::present_world3d_contour_suite(cam);
    plugin_mark("seed-contour");
    plugin_mark("earth-atmo");
  }
  plugin_mark("seed-china-flags");
  plugin_mark("orbit-china");
}

void seed_world3d_true_earth_globe(HarnessShell& browser,
                                   content::Scene3dPresenter* cam) {
  if (!cam) {
    return;
  }
  plugin_mark("seed-globe-begin");
  content::OrbitFrame* orbit = browser.orbit_frame();
  plugin::World3dLookSeed seed;
  if (!plugin::apply_world3d_look(cam, orbit, plugin::World3dLook::kGlobe,
                                 &seed)) {
    return;
  }
  (void)plugin::present_world3d_contour_suite(cam);
  plugin_mark("seed-contour");
  plugin_mark("seed-globe-flags");
  if (orbit) {
    plugin::apply_world3d_globe_flythrough(
        orbit, 0.42f, seed.globe_china_yaw, seed.globe_china_pitch,
        &cam->atmosphere_session().globe_pass(), &cam->atmosphere_session());
  }
  plugin_mark("earth-atmo");
  plugin_mark("orbit-globe");
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
            plugin_mark("earth-tiles");
            tiles_ok = true;
          } else {
            plugin_mark("earth-tiles-attach-fail");
            tiles_ok = true;  // attempted
          }
        }
      }
    }
  }
  if (!tiles_ok) {
    plugin_mark("earth-tiles-skip");
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
      const float z_m = (i * 3 + 2 < cloud.xyz.size()) ? cloud.xyz[i * 3 + 2]
                                                       : 200.f;
      float rf = 0.f;
      float gf = 0.f;
      float bf = 0.f;
      vista::hypsometric_rgb(z_m, &rf, &gf, &bf);
      rgba_fallback[i * 4] = static_cast<uint8_t>(rf * 255.f + 0.5f);
      rgba_fallback[i * 4 + 1] = static_cast<uint8_t>(gf * 255.f + 0.5f);
      rgba_fallback[i * 4 + 2] = static_cast<uint8_t>(bf * 255.f + 0.5f);
      rgba_fallback[i * 4 + 3] = 255;
    }
    rgba = rgba_fallback.data();
  }
  const int n = static_cast<int>(cloud.point_count());
  cam->set_overlay_pointcloud(xyz_lifted.data(), n, rgba);
  plugin_mark("overlay-ok");
}

}  // namespace detail
}  // namespace plugin

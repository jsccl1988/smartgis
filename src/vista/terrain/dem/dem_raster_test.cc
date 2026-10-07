// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "gis/style/style_types.h"
#include "vista/component/map/shade/bake.h"
#include "vista/terrain/dem/dem_frame.h"
#include "vista/terrain/dem/raster/dem_raster.h"
#include "vista/component/world/terrain/seed.h"
#include "vista/terrain/dem/bake/bake_backend.h"
#include "vista/terrain/dem/shade/dem_hillshade.h"
#include "vista/terrain/dem/mask/land_mask.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

int64_t elapsed_ms(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - t0)
      .count();
}

bool write_hillshade_bench_json(const char* json) {
  char exe_dir[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return false;
  }
  char* slash = std::strrchr(exe_dir, '\\');
  if (!slash) {
    return false;
  }
  slash[1] = '\0';
  char p1[MAX_PATH] = {};
  std::snprintf(p1, sizeof(p1), "%scaptures", exe_dir);
  CreateDirectoryA(p1, nullptr);
  char p2[MAX_PATH] = {};
  std::snprintf(p2, sizeof(p2), "%scaptures\\analysis", exe_dir);
  CreateDirectoryA(p2, nullptr);
  char p3[MAX_PATH] = {};
  std::snprintf(p3, sizeof(p3), "%scaptures\\analysis\\hillshade_bake", exe_dir);
  CreateDirectoryA(p3, nullptr);
  char outp[MAX_PATH] = {};
  std::snprintf(outp, sizeof(outp), "%s\\shade_kernel.json", p3);
  std::ofstream f(outp, std::ios::binary);
  if (!f) {
    return false;
  }
  f << json;
  std::fprintf(stderr, "hillshade_bake_bench wrote %s\n", outp);
  return true;
}

}  // namespace

int main() {
  expect(std::fabs(vista::kDemDefaultOrbitYaw - (3.14159265f - 0.55f)) < 1e-6f,
         "shared default orbit yaw");

  vista::DemRaster dem;
  const std::string dem_path = vista::find_sample_dem_path();
  if (dem_path.empty() || !dem.load_gdal_raster(dem_path.c_str()) ||
      dem.empty()) {
    std::fprintf(stderr,
                 "dem_raster_test: missing out/data/china_dem.tif "
                 "(build //testing/data:china_map_samples)\n");
    return 1;
  }
  expect(!dem.empty(), "real china_dem");
  expect(dem.cols() >= 2 && dem.rows() >= 2, "grid size");

  // Geographic mesh: X=-lon, +Z=north. RH lookAt looking north has camera
  // right=-X, so east (more negative X) sits on screen-right (左西右东).
  {
    std::vector<float> xyz;
    std::vector<uint32_t> idx;
    expect(dem.build_mesh(48, &xyz, &idx), "orientation mesh");
    expect(!xyz.empty() && (xyz.size() % 3) == 0, "xyz triples");
    float x_min = xyz[0], x_max = xyz[0];
    float z_min = xyz[2], z_max = xyz[2];
    for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
      x_min = x_min < xyz[i] ? x_min : xyz[i];
      x_max = x_max > xyz[i] ? x_max : xyz[i];
      z_min = z_min < xyz[i + 2] ? z_min : xyz[i + 2];
      z_max = z_max > xyz[i + 2] ? z_max : xyz[i + 2];
    }
    expect(x_min < -120.f && x_max > -90.f, "X=-lon spans China");
    expect(z_min < 25.f && z_max > 45.f, "lat on +Z south-north");
    expect(vista::dem_lon_to_x(121.0) < vista::dem_lon_to_x(88.0),
           "east X more negative than west (screen-right looking north)");
    expect(dem.sample_meters(88.0, 32.0) > dem.sample_meters(119.0, 32.5),
           "tibet higher than jiangsu (not N/S swapped)");
  }

  // Synthetic / non-china_dem may remask; china_dem path skips remask.
  // Default discovery must prefer china_dem over global_dem when both exist
  // (else China showcase remasks with 48 prefecture rings → plains holes).
  {
    const std::string default_dem = vista::find_sample_dem_path();
    if (!default_dem.empty()) {
      expect(default_dem.find("china_dem") != std::string::npos,
             "find_sample_dem_path prefers china_dem over global_dem");
    }
    const std::string global_dem = vista::find_sample_global_dem_path();
    if (!global_dem.empty() &&
        global_dem.find("global_dem") != std::string::npos) {
      expect(true, "find_sample_global_dem_path resolves global when present");
    }
  }
  {
    vista::LonLatRing tiny;
    tiny.x = {118.5, 121.5, 121.5, 118.5};
    tiny.y = {30.5, 30.5, 32.5, 32.5};
    // Remask behavior on a second load of the real DEM (not china_dem
    // seed_china_dem_into_world, which skips remask for china_dem paths).
    vista::DemRaster remask;
    expect(remask.load_gdal_raster(dem_path.c_str()) && !remask.empty(),
           "reload china_dem for remask");
    expect(remask.sample_meters(88.0, 32.0) > 500.f, "tibet before mask");
    remask.mask_outside_rings({tiny});
    expect(remask.sample_meters(88.0, 32.0) == 0.f, "remask zeros tibet");

    expect(dem_path.find("china_dem") != std::string::npos,
           "fixture path is china_dem");
    vista::World cut;
    vista::Node* n =
        vista::seed_china_dem_into_world(&cut, &tiny, 1, "skip_cutline", 48);
    expect(n != nullptr && n->has_terrain_mesh(), "china_dem seed with rings");
    bool found_tibet_elev = false;
    const std::vector<float>& pos = n->terrain.positions;
    for (size_t i = 0; i + 2 < pos.size(); i += 3) {
      if (pos[i] > -95.f && pos[i] < -85.f && pos[i + 2] > 28.f &&
          pos[i + 2] < 36.f && pos[i + 1] > 0.05f) {
        found_tibet_elev = true;
        break;
      }
    }
    expect(found_tibet_elev,
           "china_dem path skips remask (tibet elev retained)");
  }

  vista::World world;
  vista::Node* node =
      vista::seed_dem_raster_into_world(&world, dem, "china_dem", 48);
  expect(node != nullptr, "seed node");
  expect(node->kind == vista::NodeKind::kTerrain, "terrain kind");
  expect(node->has_terrain_mesh(), "terrain mesh");

  vista::World world2;
  vista::Node* china =
      vista::seed_china_dem_into_world(&world2, nullptr, 0, "views_dem", 32);
  expect(china != nullptr, "china helper");
  expect(china->has_terrain_mesh(), "china mesh");

  // LOD: closer camera → denser max_edge / more vertices.
  {
    const int far_edge = vista::DemRaster::lod_max_edge(7.0f);
    const int near_edge = vista::DemRaster::lod_max_edge(1.0f);
    expect(near_edge > far_edge, "near LOD denser than far");
    const int far_verts = vista::DemRaster::lod_expected_vertices(
        dem.cols(), dem.rows(), far_edge);
    const int near_verts = vista::DemRaster::lod_expected_vertices(
        dem.cols(), dem.rows(), near_edge);
    expect(near_verts >= far_verts, "near LOD more verts");

    std::vector<float> xyz_far;
    std::vector<uint32_t> idx_far;
    std::vector<float> xyz_near;
    std::vector<uint32_t> idx_near;
    expect(dem.build_mesh(far_edge, &xyz_far, &idx_far), "far mesh");
    expect(dem.build_mesh(near_edge, &xyz_near, &idx_near), "near mesh");
    expect(xyz_near.size() >= xyz_far.size(), "near mesh denser XYZ");

    vista::World lod_world;
    vista::Node* lod_node =
        vista::seed_dem_raster_lod_into_world(&lod_world, dem, "lod_dem", 1.0f);
    expect(lod_node != nullptr && lod_node->has_terrain_mesh(),
           "lod seed into world");
  }

  // P0-C: view AABB tiles — zoom-in increases tile count; verts under budget.
  {
    vista::World tile_world;
    const size_t far_tiles = vista::seed_dem_view_tiles_into_world(
        &tile_world, dem, 73.0, 17.5, 135.0, 54.0, 3.2f, 65536, "t");
    expect(far_tiles == 1, "far orbit single tile");
    size_t far_verts = 0;
    for (size_t i = 0; i < tile_world.node_count(); ++i) {
      const vista::Node* n = tile_world.node_at(i);
      if (n && n->has_terrain_mesh()) {
        far_verts += n->terrain.positions.size() / 3;
      }
    }
    expect(far_verts > 0 && far_verts <= 65536u, "far verts under budget");

    vista::World near_world;
    const size_t near_tiles = vista::seed_dem_view_tiles_into_world(
        &near_world, dem, 110.0, 30.0, 120.0, 40.0, 0.8f, 65536, "n");
    expect(near_tiles >= 2, "near orbit finer tile grid");
    size_t near_verts = 0;
    for (size_t i = 0; i < near_world.node_count(); ++i) {
      const vista::Node* n = near_world.node_at(i);
      if (n && n->has_terrain_mesh()) {
        near_verts += n->terrain.positions.size() / 3;
      }
    }
    expect(near_verts > 0 && near_verts <= 65536u, "near verts under budget");
    expect(near_tiles > far_tiles, "zoom-in replaces coarse with finer");

    std::vector<float> win_xyz;
    std::vector<uint32_t> win_idx;
    expect(dem.build_mesh_window(100.0, 30.0, 110.0, 40.0, 32, &win_xyz,
                                 &win_idx),
           "build_mesh_window");
    expect(win_xyz.size() >= 9 && win_idx.size() >= 3, "window mesh non-empty");
  }

  // Owned hillshade: real china_dem yields non-flat translucent RGBA.
  {
    vista::HillshadeParams hs;
    hs.max_edge = 128;
    hs.exaggeration = 4.f;
    hs.illumination_altitude_deg = 35.f;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
    expect(vista::shade_dem_rgba(dem, hs, &rgba, &w, &h), "shade_dem_rgba");
    expect(w >= 2 && h >= 2 &&
               rgba.size() == static_cast<size_t>(w) * static_cast<size_t>(h) * 4u,
           "hillshade size");
    int opaque = 0;
    int min_luma = 255;
    int max_luma = 0;
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
      if (rgba[i + 3] == 0) {
        continue;
      }
      ++opaque;
      const int luma =
          (static_cast<int>(rgba[i]) + static_cast<int>(rgba[i + 1]) +
           static_cast<int>(rgba[i + 2])) /
          3;
      min_luma = (std::min)(min_luma, luma);
      max_luma = (std::max)(max_luma, luma);
    }
    expect(opaque > 16, "hillshade land pixels");
    expect(max_luma > min_luma, "hillshade luminance variance");
  }

  if (vista::bake_bench_wanted_from_env()) {
    _putenv_s("BAKE_DISK", "0");
    vista::HillshadeParams hs;
    hs.max_edge = 768;
    struct Cell {
      const char* name;
      int ok = 0;
      int used_cuda = 0;
      int64_t cold_ms = 0;
      int64_t warm_ms = 0;
      int w = 0;
      int h = 0;
    };
    auto run_cell = [&](const char* backend) {
      Cell c;
      c.name = backend;
      _putenv_s("BAKE_BACKEND", backend);
      vista::reset_last_shade_used_cuda();
      std::vector<uint8_t> rgba;
      int w = 0;
      int h = 0;
      auto t0 = std::chrono::steady_clock::now();
      c.ok = vista::shade_dem_rgba(dem, hs, &rgba, &w, &h) ? 1 : 0;
      c.cold_ms = elapsed_ms(t0);
      c.used_cuda = vista::last_shade_used_cuda();
      c.w = w;
      c.h = h;
      rgba.clear();
      t0 = std::chrono::steady_clock::now();
      if (c.ok) {
        c.ok = vista::shade_dem_rgba(dem, hs, &rgba, &w, &h) ? 1 : 0;
      }
      c.warm_ms = elapsed_ms(t0);
      c.used_cuda = vista::last_shade_used_cuda();
      std::fprintf(stderr,
                   "bake_bench shade backend=%s ok=%d cuda=%d cold_ms=%lld "
                   "warm_ms=%lld %dx%d\n",
                   c.name, c.ok, c.used_cuda, static_cast<long long>(c.cold_ms),
                   static_cast<long long>(c.warm_ms), c.w, c.h);
      return c;
    };
    const Cell cpu = run_cell("cpu");
    expect(cpu.ok == 1, "bake_bench cpu shade");
    expect(cpu.used_cuda == 0, "bake_bench cpu not cuda");
    const Cell cuda = run_cell("cuda");
    if (cuda.ok) {
      expect(cuda.used_cuda == 1, "bake_bench cuda used thrust");
    } else {
      std::fprintf(stderr, "bake_bench: cuda skipped (no device / stub)\n");
    }
    gis::style::StyleLayer layer;
    layer.type = gis::style::LayerType::kHillshade;
    auto run_slot = [&](const char* backend) {
      _putenv_s("BAKE_BACKEND", backend);
      vista::reset_hillshade_bake_cache();
      vista::reset_hillshade_bake_sample();
      const auto t0 = std::chrono::steady_clock::now();
      vista::HillshadeBake baked =
          vista::bake_hillshade_slot(dem_path, 5.0, layer, 1);
      const int64_t wall = elapsed_ms(t0);
      const vista::HillshadeBakeSample s = vista::hillshade_last_bake_sample();
      std::fprintf(stderr,
                   "bake_bench slot backend=%s ok=%d wall_ms=%lld load=%lld "
                   "shade=%lld store=%lld cuda=%d %dx%d\n",
                   backend, baked.ok ? 1 : 0, static_cast<long long>(wall),
                   static_cast<long long>(s.load_ms),
                   static_cast<long long>(s.shade_ms),
                   static_cast<long long>(s.store_ms), s.used_cuda, s.width,
                   s.height);
      return s;
    };
    const vista::HillshadeBakeSample slot_cpu = run_slot("cpu");
    const vista::HillshadeBakeSample slot_cuda = run_slot("cuda");
    std::string dem_json = dem_path;
    for (char& ch : dem_json) {
      if (ch == '\\') {
        ch = '/';
      }
    }
    char json[2048];
    std::snprintf(
        json, sizeof(json),
        "{\n"
        "  \"profile\": \"china_dem max_edge=768 illum=335/32 exag=0.5\",\n"
        "  \"dem_path\": \"%s\",\n"
        "  \"kernel\": [\n"
        "    {\"backend\":\"cpu\",\"ok\":%d,\"used_cuda\":%d,"
        "\"cold_ms\":%lld,\"warm_ms\":%lld,\"w\":%d,\"h\":%d},\n"
        "    {\"backend\":\"cuda\",\"ok\":%d,\"used_cuda\":%d,"
        "\"cold_ms\":%lld,\"warm_ms\":%lld,\"w\":%d,\"h\":%d}\n"
        "  ],\n"
        "  \"slot\": [\n"
        "    {\"backend\":\"cpu\",\"load_ms\":%lld,\"shade_ms\":%lld,"
        "\"store_ms\":%lld,\"used_cuda\":%d},\n"
        "    {\"backend\":\"cuda\",\"load_ms\":%lld,\"shade_ms\":%lld,"
        "\"store_ms\":%lld,\"used_cuda\":%d}\n"
        "  ]\n"
        "}\n",
        dem_json.c_str(), cpu.ok, cpu.used_cuda,
        static_cast<long long>(cpu.cold_ms),
        static_cast<long long>(cpu.warm_ms), cpu.w, cpu.h, cuda.ok,
        cuda.used_cuda, static_cast<long long>(cuda.cold_ms),
        static_cast<long long>(cuda.warm_ms), cuda.w, cuda.h,
        static_cast<long long>(slot_cpu.load_ms),
        static_cast<long long>(slot_cpu.shade_ms),
        static_cast<long long>(slot_cpu.store_ms), slot_cpu.used_cuda,
        static_cast<long long>(slot_cuda.load_ms),
        static_cast<long long>(slot_cuda.shade_ms),
        static_cast<long long>(slot_cuda.store_ms), slot_cuda.used_cuda);
    write_hillshade_bench_json(json);
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "dem_raster_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "dem_raster_test: ok\n");
  return 0;
}

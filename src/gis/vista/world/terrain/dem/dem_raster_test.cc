// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/world/terrain/dem/dem_frame.h"
#include "gis/vista/world/terrain/process/dem_hillshade.h"
#include "gis/vista/world/terrain/dem/dem_raster.h"
#include "gis/vista/world/terrain/process/land_mask.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  expect(std::fabs(gis::kDemDefaultOrbitYaw - (3.14159265f - 0.55f)) < 1e-6f,
         "shared default orbit yaw");

  gis::DemRaster dem;
  dem.fill_synthetic_china();
  expect(!dem.empty(), "synthetic dem");
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
    expect(gis::dem_lon_to_x(121.0) < gis::dem_lon_to_x(88.0),
           "east X more negative than west (screen-right looking north)");
    expect(dem.sample_meters(88.0, 32.0) > dem.sample_meters(119.0, 32.5),
           "tibet higher than jiangsu (not N/S swapped)");
  }

  // Synthetic / non-china_dem may remask; china_dem path skips remask.
  {
    gis::LonLatRing tiny;
    tiny.x = {118.5, 121.5, 121.5, 118.5};
    tiny.y = {30.5, 30.5, 32.5, 32.5};
    gis::DemRaster synth;
    synth.fill_synthetic_china();
    expect(synth.sample_meters(88.0, 32.0) > 500.f, "synth tibet before mask");
    synth.mask_outside_rings({tiny});
    expect(synth.sample_meters(88.0, 32.0) == 0.f,
           "synthetic remask zeros tibet");

    const std::string path = gis::find_sample_dem_path();
    if (!path.empty() && path.find("china_dem") != std::string::npos) {
      gis::World cut;
      gis::Node* n =
          gis::seed_china_dem_into_world(&cut, &tiny, 1, "skip_cutline", 48);
      expect(n != nullptr && n->has_terrain_mesh(),
             "china_dem seed with rings");
      bool found_tibet_elev = false;
      const std::vector<float>& pos = n->terrain_positions;
      for (size_t i = 0; i + 2 < pos.size(); i += 3) {
        if (pos[i] > -95.f && pos[i] < -85.f && pos[i + 2] > 28.f &&
            pos[i + 2] < 36.f && pos[i + 1] > 0.05f) {
          found_tibet_elev = true;
          break;
        }
      }
      expect(found_tibet_elev,
             "china_dem path skips remask (tibet elev retained)");
    } else {
      std::fprintf(stdout,
                   "dem_raster_test: no china_dem fixture; cutline skip "
                   "assert deferred\n");
    }
  }

  gis::World world;
  gis::Node* node =
      gis::seed_dem_raster_into_world(&world, dem, "china_dem", 48);
  expect(node != nullptr, "seed node");
  expect(node->kind == gis::NodeKind::kTerrain, "terrain kind");
  expect(node->has_terrain_mesh(), "terrain mesh");

  gis::World world2;
  gis::Node* china =
      gis::seed_china_dem_into_world(&world2, nullptr, 0, "views_dem", 32);
  expect(china != nullptr, "china helper");
  expect(china->has_terrain_mesh(), "china mesh");

  // LOD: closer camera → denser max_edge / more vertices.
  {
    const int far_edge = gis::DemRaster::lod_max_edge(7.0f);
    const int near_edge = gis::DemRaster::lod_max_edge(1.0f);
    expect(near_edge > far_edge, "near LOD denser than far");
    const int far_verts = gis::DemRaster::lod_expected_vertices(
        dem.cols(), dem.rows(), far_edge);
    const int near_verts = gis::DemRaster::lod_expected_vertices(
        dem.cols(), dem.rows(), near_edge);
    expect(near_verts >= far_verts, "near LOD more verts");

    std::vector<float> xyz_far;
    std::vector<uint32_t> idx_far;
    std::vector<float> xyz_near;
    std::vector<uint32_t> idx_near;
    expect(dem.build_mesh(far_edge, &xyz_far, &idx_far), "far mesh");
    expect(dem.build_mesh(near_edge, &xyz_near, &idx_near), "near mesh");
    expect(xyz_near.size() >= xyz_far.size(), "near mesh denser XYZ");

    gis::World lod_world;
    gis::Node* lod_node =
        gis::seed_dem_raster_lod_into_world(&lod_world, dem, "lod_dem", 1.0f);
    expect(lod_node != nullptr && lod_node->has_terrain_mesh(),
           "lod seed into world");
  }

  // P0-C: view AABB tiles — zoom-in increases tile count; verts under budget.
  {
    gis::World tile_world;
    const size_t far_tiles = gis::seed_dem_view_tiles_into_world(
        &tile_world, dem, 73.0, 17.5, 135.0, 54.0, 3.2f, 65536, "t");
    expect(far_tiles == 1, "far orbit single tile");
    size_t far_verts = 0;
    for (size_t i = 0; i < tile_world.node_count(); ++i) {
      const gis::Node* n = tile_world.node_at(i);
      if (n && n->has_terrain_mesh()) {
        far_verts += n->terrain_positions.size() / 3;
      }
    }
    expect(far_verts > 0 && far_verts <= 65536u, "far verts under budget");

    gis::World near_world;
    const size_t near_tiles = gis::seed_dem_view_tiles_into_world(
        &near_world, dem, 110.0, 30.0, 120.0, 40.0, 0.8f, 65536, "n");
    expect(near_tiles >= 2, "near orbit finer tile grid");
    size_t near_verts = 0;
    for (size_t i = 0; i < near_world.node_count(); ++i) {
      const gis::Node* n = near_world.node_at(i);
      if (n && n->has_terrain_mesh()) {
        near_verts += n->terrain_positions.size() / 3;
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

  // Owned hillshade: synthetic DEM yields non-flat translucent RGBA.
  {
    gis::HillshadeParams hs;
    hs.max_edge = 128;
    hs.exaggeration = 4.f;
    hs.illumination_altitude_deg = 35.f;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
    expect(gis::shade_dem_rgba(dem, hs, &rgba, &w, &h), "shade_dem_rgba");
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

  if (g_fails != 0) {
    std::fprintf(stderr, "dem_raster_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "dem_raster_test: ok\n");
  return 0;
}

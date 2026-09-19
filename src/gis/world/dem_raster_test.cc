// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/world/dem_frame.h"
#include "gis/world/dem_raster.h"
#include "gis/world/land_mask.h"

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

  if (g_fails != 0) {
    std::fprintf(stderr, "dem_raster_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "dem_raster_test: ok\n");
  return 0;
}

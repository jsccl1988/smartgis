// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/world/dem_raster.h"

#include <cstdio>
#include <cstdint>
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
  gis::DemRaster dem;
  dem.fill_synthetic_china();
  expect(!dem.empty(), "synthetic dem");
  expect(dem.cols() >= 2 && dem.rows() >= 2, "grid size");

  // Geographic axes: +X east, +Z north (leftover Y-up). Mesh must keep
  // 上北下南 / 左西右东 so Views orbit from south frames north at screen top.
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
    expect(x_min < 90.f && x_max > 120.f, "lon span west-east");
    expect(z_min < 25.f && z_max > 45.f, "lat span south-north on +Z");
    expect(dem.sample_meters(88.0, 32.0) > dem.sample_meters(119.0, 32.5),
           "tibet higher than jiangsu (not N/S swapped)");
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

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/world/dem_raster.h"

#include <cstdio>

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

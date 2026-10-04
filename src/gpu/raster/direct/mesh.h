// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_RASTER_DIRECT_MESH_H_
#define GPU_RASTER_DIRECT_MESH_H_

// Process-lifetime China DEM mesh for the Scene3d direct raster (real GeoTIFF).

#include <cstdint>
#include <vector>

namespace gpu {
namespace detail {

// Built once per process from find_sample_dem_path() + build_mesh(48).
// ready stays false when out/data/china_dem.tif (or override) is missing.
struct SyntheticDemMesh {
  std::vector<float> xyz;
  std::vector<uint32_t> indices;
  float miny = 0.f;
  float maxy = 0.f;
  float cx = 0.f;
  float cy = 0.f;
  float cz = 0.f;
  float span = 1.f;
  bool ready = false;
};

const SyntheticDemMesh& synthetic_dem_mesh();

}  // namespace detail
}  // namespace gpu

#endif  // GPU_RASTER_DIRECT_MESH_H_

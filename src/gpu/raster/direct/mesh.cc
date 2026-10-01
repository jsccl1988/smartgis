// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/raster/direct/mesh.h"

#include "gis/vista/world/terrain/dem/dem_raster.h"

#include <algorithm>

namespace gpu {
namespace detail {

const SyntheticDemMesh& synthetic_dem_mesh() {
  static const SyntheticDemMesh mesh = [] {
    SyntheticDemMesh built;
    gis::DemRaster dem;
    dem.fill_synthetic_china();
    if (!dem.build_mesh(48, &built.xyz, &built.indices) ||
        built.xyz.size() < 9 || built.indices.size() < 3) {
      return built;
    }
    float minx = built.xyz[0];
    float maxx = built.xyz[0];
    float minz = built.xyz[2];
    float maxz = built.xyz[2];
    built.miny = built.xyz[1];
    built.maxy = built.xyz[1];
    for (size_t i = 0; i + 2 < built.xyz.size(); i += 3) {
      minx = (std::min)(minx, built.xyz[i]);
      maxx = (std::max)(maxx, built.xyz[i]);
      built.miny = (std::min)(built.miny, built.xyz[i + 1]);
      built.maxy = (std::max)(built.maxy, built.xyz[i + 1]);
      minz = (std::min)(minz, built.xyz[i + 2]);
      maxz = (std::max)(maxz, built.xyz[i + 2]);
    }
    built.cx = 0.5f * (minx + maxx);
    built.cy = 0.5f * (built.miny + built.maxy);
    built.cz = 0.5f * (minz + maxz);
    built.span = (std::max)(maxx - minx, (std::max)(maxz - minz, 1.f));
    built.ready = true;
    return built;
  }();
  return mesh;
}

}  // namespace detail
}  // namespace gpu

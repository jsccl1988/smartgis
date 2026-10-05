// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/terrain/lod.h"

#include "vista/terrain/dem/dem_raster.h"

namespace vista {

int terrain_lod_max_edge(float camera_distance) {
  return DemRaster::lod_max_edge(camera_distance);
}

int terrain_lod_cache_key(float camera_distance) {
  return dem_seed_cache_key(camera_distance);
}

int terrain_lod_tin_stride(float camera_distance) {
  // Far → keep 1/8; mid → 1/4; near → 1/2; very near → full.
  if (camera_distance < 1.2f) {
    return 1;
  }
  if (camera_distance < 2.4f) {
    return 2;
  }
  if (camera_distance < 4.0f) {
    return 4;
  }
  return 8;
}

int terrain_lod_surface_edge(float camera_distance) {
  return terrain_lod_max_edge(camera_distance);
}

int terrain_lod_tin_cache_key(float camera_distance) {
  return terrain_lod_tin_stride(camera_distance) * 1000 +
         terrain_lod_surface_edge(camera_distance);
}

}  // namespace vista

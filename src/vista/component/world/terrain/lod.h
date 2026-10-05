// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// IR-side discrete LOD for DEM terrain.
// Raster: lod_max_edge + view tiles. TIN: triangle stride thin.
// Surface: heightfield sample density (DemHeightField / DemRaster build_mesh).
// Gap (this wave): no clipmap / CDLOD / GPU tess / continuous SSE.

#ifndef VISTA_COMPONENT_WORLD_TERRAIN_LOD_H_
#define VISTA_COMPONENT_WORLD_TERRAIN_LOD_H_

#include <cstdint>

#include "vista/vista_export.h"

namespace vista {

// How a TerrainPayload was produced (seed stamps this; GPU path ignores it).
enum class TerrainSource : uint8_t {
  kUnknown = 0,
  kRaster = 1,
  kTin = 2,
  kSurface = 3,
};

// Max DEM mesh edge length for |camera_distance| (forwards DemRaster).
VISTA_EXPORT int terrain_lod_max_edge(float camera_distance);

// Cache / generation key: lod_max_edge * 10 + grid_hint (1 far, 2 near).
// Same semantics as dem_seed_cache_key.
VISTA_EXPORT int terrain_lod_cache_key(float camera_distance);

// TIN triangle keep stride: 1 = full mesh; 2/4/8 = keep every Nth triangle.
// Closer camera → denser (smaller stride). Discrete buckets only.
VISTA_EXPORT int terrain_lod_tin_stride(float camera_distance);

// Surface-interp sample edge for DemHeightField / DemRaster rebuilds.
// Aliases terrain_lod_max_edge (same discrete buckets this wave).
VISTA_EXPORT int terrain_lod_surface_edge(float camera_distance);

// Cache key for TIN LOD: stride * 1000 + surface_edge (avoids raster key clash).
VISTA_EXPORT int terrain_lod_tin_cache_key(float camera_distance);

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_TERRAIN_LOD_H_

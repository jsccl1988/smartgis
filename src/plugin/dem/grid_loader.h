// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_DEM_GRID_LOADER_H
#define PLUGIN_DEM_GRID_LOADER_H

#include "algorithm/geo/geometry.h"

#if !defined(DEM_LOADER_API)
#if defined(PLUGIN_DEM_EXPORTS)
#define DEM_LOADER_API __declspec(dllexport)
#else
#define DEM_LOADER_API
#endif
#endif

using namespace geo;

namespace plugin {

// Origin and cell size applied after GDAL reads band 1 as float heights.
struct GridLoadOptions {
  float x_start = 0.f;
  float y_start = 0.f;
  float z_start = 0.f;
  float x_scale = 1.f;
  float y_scale = 1.f;
  float z_scale = 1.f;
};

// Load a height raster via GDAL and build a regular-grid Smt3DSurface.
DEM_LOADER_API long load_heightmap_grid(const char* file_name,
                                        const GridLoadOptions& options,
                                        Smt3DSurface* out_surf);

}  // namespace plugin

#endif  // PLUGIN_DEM_GRID_LOADER_H

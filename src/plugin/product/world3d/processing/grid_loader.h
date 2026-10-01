// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_GRID_LOADER_H_
#define PLUGIN_WORLD3D_GRID_LOADER_H_

#include "plugin/product/world3d/world3d_export.h"
#include "gis/kernel/geo/mesh/geometry.h"

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
WORLD3D_LOADER_API long load_heightmap_grid(const char* file_name,
                                        const GridLoadOptions& options,
                                        Smt3DSurface* out_surf);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_GRID_LOADER_H_

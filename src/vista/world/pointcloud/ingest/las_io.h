// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_LAS_IO_H_
#define GIS_VISTA_WORLD_POINTCLOUD_LAS_IO_H_

#include <cstddef>
#include <cstdint>

#include "vista/vista_export.h"
#include "vista/world/pointcloud/point_cloud.h"

namespace vista {

// Load options for ASPRS LAS / LAZ (LAZ via third_party LASzip).
struct LasLoadOptions {
  // Hard cap after stride (P0 safety).
  size_t max_points = 500000;
  // Keep every Nth point (1 = all).
  size_t stride = 1;
};

// Reads LAS 1.2/1.4 (and LAZ) point formats 0/2/3 into |out|.
// Returns false and sets out->error on failure.
VISTA_EXPORT bool load_las_file(const char* path, const LasLoadOptions& options,
                              PointCloud* out);

}  // namespace vista

#endif  // GIS_VISTA_WORLD_POINTCLOUD_LAS_IO_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_LOD_H_
#define GIS_VISTA_WORLD_POINTCLOUD_LOD_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gis/gis_export.h"
#include "gis/vista/world/pointcloud/buffer/point_cloud.h"

namespace gis {

// Builds a unibn octree over |cloud| and selects up to |max_points| samples
// biased toward |focus| (world XYZ). Empty focus_radius → whole cloud thin.
struct PointCloudLodOptions {
  size_t max_points = 200000;
  double focus_x = 0;
  double focus_y = 0;
  double focus_z = 0;
  // 0 = ignore focus; use uniform stride over full cloud.
  double focus_radius = 0;
};

// Returns indices into cloud.xyz. Uses unibn Octree when available.
GIS_EXPORT bool select_point_cloud_lod(const PointCloud& cloud,
                                       const PointCloudLodOptions& options,
                                       std::vector<uint32_t>* out_indices);

}  // namespace gis

#endif  // GIS_VISTA_WORLD_POINTCLOUD_LOD_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_POINTCLOUD_LOD_H_
#define VISTA_COMPONENT_WORLD_POINTCLOUD_LOD_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/vista_export.h"
#include "vista/assets/pointcloud/point_cloud.h"

namespace vista {

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
VISTA_EXPORT bool select_point_cloud_lod(const PointCloud& cloud,
                                       const PointCloudLodOptions& options,
                                       std::vector<uint32_t>* out_indices);

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_POINTCLOUD_LOD_H_

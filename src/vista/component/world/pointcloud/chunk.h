// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_POINTCLOUD_CHUNK_H_
#define VISTA_COMPONENT_WORLD_POINTCLOUD_CHUNK_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/vista_export.h"
#include "vista/assets/pointcloud/point_cloud.h"

namespace vista {

// One spatial bucket of point indices for frustum cull / LOD (P1).
struct PointCloudChunk {
  double min_x = 0;
  double min_y = 0;
  double min_z = 0;
  double max_x = 0;
  double max_y = 0;
  double max_z = 0;
  std::vector<uint32_t> indices;  // into PointCloud::xyz / 3
};

struct PointCloudChunkOptions {
  // Target cells along the longest AABB axis (clamped).
  int grid_axis = 8;
  // Soft cap on points kept per chunk after uniform stride.
  size_t max_points_per_chunk = 50000;
};

// Partition |cloud| into AABB chunks. Empty cloud → false.
VISTA_EXPORT bool build_point_cloud_chunks(const PointCloud& cloud,
                                         const PointCloudChunkOptions& options,
                                         std::vector<PointCloudChunk>* out);

}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_POINTCLOUD_CHUNK_H_

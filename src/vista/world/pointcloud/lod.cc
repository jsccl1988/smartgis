// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/pointcloud/lod.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Octree.hpp"

namespace vista {
namespace {

struct Vec3f {
  float x = 0;
  float y = 0;
  float z = 0;
};

}  // namespace

bool select_point_cloud_lod(const PointCloud& cloud,
                            const PointCloudLodOptions& options,
                            std::vector<uint32_t>* out_indices) {
  if (!out_indices) {
    return false;
  }
  out_indices->clear();
  const size_t n = cloud.point_count();
  if (n == 0) {
    return false;
  }
  const size_t budget =
      options.max_points == 0 ? n : std::min(options.max_points, n);

  if (budget >= n) {
    out_indices->resize(n);
    for (size_t i = 0; i < n; ++i) {
      (*out_indices)[i] = static_cast<uint32_t>(i);
    }
    return true;
  }

  // Uniform stride when no focus radius (cheap path).
  if (options.focus_radius <= 0.0) {
    const size_t stride = (n + budget - 1) / budget;
    out_indices->reserve(budget);
    for (size_t i = 0; i < n && out_indices->size() < budget; i += stride) {
      out_indices->push_back(static_cast<uint32_t>(i));
    }
    return !out_indices->empty();
  }

  std::vector<Vec3f> pts;
  pts.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    pts.push_back(Vec3f{cloud.xyz[i * 3], cloud.xyz[i * 3 + 1],
                        cloud.xyz[i * 3 + 2]});
  }
  unibn::Octree<Vec3f> tree;
  tree.initialize(pts);

  const Vec3f q{static_cast<float>(options.focus_x),
                static_cast<float>(options.focus_y),
                static_cast<float>(options.focus_z)};
  std::vector<uint32_t> neighbors;
  tree.radiusNeighbors<unibn::L2Distance<Vec3f>>(
      q, static_cast<float>(options.focus_radius), neighbors);

  if (neighbors.empty()) {
    // Fall back to uniform if focus misses.
    const size_t stride = (n + budget - 1) / budget;
    for (size_t i = 0; i < n && out_indices->size() < budget; i += stride) {
      out_indices->push_back(static_cast<uint32_t>(i));
    }
    return !out_indices->empty();
  }

  if (neighbors.size() <= budget) {
    out_indices->assign(neighbors.begin(), neighbors.end());
    return true;
  }
  const size_t stride = (neighbors.size() + budget - 1) / budget;
  out_indices->reserve(budget);
  for (size_t i = 0; i < neighbors.size() && out_indices->size() < budget;
       i += stride) {
    out_indices->push_back(neighbors[i]);
  }
  return !out_indices->empty();
}

}  // namespace vista

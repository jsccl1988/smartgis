// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/pointcloud/process/chunk.h"

#include <algorithm>
#include <cmath>

namespace vista {

bool build_point_cloud_chunks(const PointCloud& cloud,
                              const PointCloudChunkOptions& options,
                              std::vector<PointCloudChunk>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  const size_t n = cloud.point_count();
  if (n == 0 || cloud.xyz.size() < 3) {
    return false;
  }

  const double dx = cloud.max_x - cloud.min_x;
  const double dy = cloud.max_y - cloud.min_y;
  const double dz = cloud.max_z - cloud.min_z;
  const double span = std::max(dx, std::max(dy, dz));
  int axis = options.grid_axis < 1 ? 1 : options.grid_axis;
  if (axis > 32) {
    axis = 32;
  }
  const double cell = span > 0.0 ? (span / static_cast<double>(axis)) : 1.0;
  const double inv = 1.0 / cell;

  const int nx =
      std::max(1, static_cast<int>(std::floor(dx * inv)) + 1);
  const int ny =
      std::max(1, static_cast<int>(std::floor(dy * inv)) + 1);
  const int nz =
      std::max(1, static_cast<int>(std::floor(dz * inv)) + 1);
  const size_t cells = static_cast<size_t>(nx) * static_cast<size_t>(ny) *
                       static_cast<size_t>(nz);
  std::vector<PointCloudChunk> bins(cells);

  auto cell_index = [&](double x, double y, double z) -> size_t {
    int ix = static_cast<int>(std::floor((x - cloud.min_x) * inv));
    int iy = static_cast<int>(std::floor((y - cloud.min_y) * inv));
    int iz = static_cast<int>(std::floor((z - cloud.min_z) * inv));
    if (ix < 0) {
      ix = 0;
    }
    if (iy < 0) {
      iy = 0;
    }
    if (iz < 0) {
      iz = 0;
    }
    if (ix >= nx) {
      ix = nx - 1;
    }
    if (iy >= ny) {
      iy = ny - 1;
    }
    if (iz >= nz) {
      iz = nz - 1;
    }
    return static_cast<size_t>(ix) +
           static_cast<size_t>(iy) * static_cast<size_t>(nx) +
           static_cast<size_t>(iz) * static_cast<size_t>(nx) *
               static_cast<size_t>(ny);
  };

  for (size_t i = 0; i < n; ++i) {
    const float x = cloud.xyz[i * 3];
    const float y = cloud.xyz[i * 3 + 1];
    const float z = cloud.xyz[i * 3 + 2];
    PointCloudChunk& c = bins[cell_index(x, y, z)];
    if (c.indices.empty()) {
      c.min_x = c.max_x = x;
      c.min_y = c.max_y = y;
      c.min_z = c.max_z = z;
    } else {
      c.min_x = std::min(c.min_x, static_cast<double>(x));
      c.min_y = std::min(c.min_y, static_cast<double>(y));
      c.min_z = std::min(c.min_z, static_cast<double>(z));
      c.max_x = std::max(c.max_x, static_cast<double>(x));
      c.max_y = std::max(c.max_y, static_cast<double>(y));
      c.max_z = std::max(c.max_z, static_cast<double>(z));
    }
    c.indices.push_back(static_cast<uint32_t>(i));
  }

  const size_t cap = options.max_points_per_chunk == 0
                         ? n
                         : options.max_points_per_chunk;
  out->reserve(bins.size());
  for (PointCloudChunk& c : bins) {
    if (c.indices.empty()) {
      continue;
    }
    if (c.indices.size() > cap) {
      const size_t stride =
          (c.indices.size() + cap - 1) / cap;
      std::vector<uint32_t> thinned;
      thinned.reserve(cap);
      for (size_t k = 0; k < c.indices.size(); k += stride) {
        thinned.push_back(c.indices[k]);
        if (thinned.size() >= cap) {
          break;
        }
      }
      c.indices.swap(thinned);
    }
    out->push_back(std::move(c));
  }
  return !out->empty();
}

}  // namespace vista

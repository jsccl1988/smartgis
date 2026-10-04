// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_VISTA_WORLD_POINTCLOUD_POINT_CLOUD_H_
#define GIS_VISTA_WORLD_POINTCLOUD_POINT_CLOUD_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vista/vista_export.h"

namespace vista {

// CPU point cloud buffer (xyz + optional RGBA). Owned by loaders / World nodes.
struct PointCloud {
  std::vector<float> xyz;       // interleaved X,Y,Z
  std::vector<uint8_t> rgba;    // optional; size == 4 * point_count when set
  double min_x = 0;
  double min_y = 0;
  double min_z = 0;
  double max_x = 0;
  double max_y = 0;
  double max_z = 0;
  std::string source_path;
  std::string error;

  size_t point_count() const { return xyz.size() / 3; }
  bool empty() const { return xyz.size() < 3; }
  bool has_color() const {
    return !rgba.empty() && rgba.size() == point_count() * 4;
  }

  void clear() {
    xyz.clear();
    rgba.clear();
    min_x = min_y = min_z = 0;
    max_x = max_y = max_z = 0;
    source_path.clear();
    error.clear();
  }

  void recompute_bounds();
};

}  // namespace vista

#endif  // GIS_VISTA_WORLD_POINTCLOUD_POINT_CLOUD_H_

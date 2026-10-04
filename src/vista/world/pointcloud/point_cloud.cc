// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/pointcloud/point_cloud.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vista {

void PointCloud::recompute_bounds() {
  if (xyz.size() < 3) {
    min_x = min_y = min_z = 0;
    max_x = max_y = max_z = 0;
    return;
  }
  double lo_x = std::numeric_limits<double>::infinity();
  double lo_y = std::numeric_limits<double>::infinity();
  double lo_z = std::numeric_limits<double>::infinity();
  double hi_x = -std::numeric_limits<double>::infinity();
  double hi_y = -std::numeric_limits<double>::infinity();
  double hi_z = -std::numeric_limits<double>::infinity();
  const size_t n = xyz.size() / 3;
  for (size_t i = 0; i < n; ++i) {
    const double x = xyz[i * 3];
    const double y = xyz[i * 3 + 1];
    const double z = xyz[i * 3 + 2];
    lo_x = std::min(lo_x, x);
    lo_y = std::min(lo_y, y);
    lo_z = std::min(lo_z, z);
    hi_x = std::max(hi_x, x);
    hi_y = std::max(hi_y, y);
    hi_z = std::max(hi_z, z);
  }
  min_x = lo_x;
  min_y = lo_y;
  min_z = lo_z;
  max_x = hi_x;
  max_y = hi_y;
  max_z = hi_z;
}

}  // namespace vista

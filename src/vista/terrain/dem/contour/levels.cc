// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/contour/levels.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/terrain/dem/contour/msquares.h"
#include "vista/terrain/dem/contour/smooth.h"
#include "vista/terrain/dem/dem_contour.h"
#include "vista/terrain/dem/bake/bake_parallel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace vista {

float pick_contour_interval_m(float zmin, float zmax) {
  const float span = zmax - zmin;
  if (!(span > 1.f)) {
    return 1.f;
  }
  const float raw = span / 12.f;
  const float mag = std::pow(10.f, std::floor(std::log10(raw)));
  const float n = raw / mag;
  float nice = 1.f;
  if (n >= 7.5f) {
    nice = 10.f;
  } else if (n >= 3.5f) {
    nice = 5.f;
  } else if (n >= 1.5f) {
    nice = 2.f;
  }
  return nice * mag;
}

namespace detail {

bool land_height_range(const float* heights, int cols, int rows,
                       float skip_below, float* zmin_out, float* zmax_out) {
  if (!heights || !zmin_out || !zmax_out || cols < 2 || rows < 2) {
    return false;
  }
  const std::size_t n =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  float zmin = 1.e9f;
  float zmax = -1.e9f;
  if (bake_rows_should_parallel(cols, rows)) {
    std::vector<float> row_min(static_cast<std::size_t>(rows), 1.e9f);
    std::vector<float> row_max(static_cast<std::size_t>(rows), -1.e9f);
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, rows, [&](int y) {
      float mn = 1.e9f;
      float mx = -1.e9f;
      const float* rowp = heights + static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(cols);
      for (int x = 0; x < cols; ++x) {
        const float hv = rowp[x];
        if (hv < skip_below) {
          continue;
        }
        mn = (std::min)(mn, hv);
        mx = (std::max)(mx, hv);
      }
      row_min[static_cast<std::size_t>(y)] = mn;
      row_max[static_cast<std::size_t>(y)] = mx;
    });
    for (int y = 0; y < rows; ++y) {
      zmin = (std::min)(zmin, row_min[static_cast<std::size_t>(y)]);
      zmax = (std::max)(zmax, row_max[static_cast<std::size_t>(y)]);
    }
  } else {
    for (std::size_t i = 0; i < n; ++i) {
      const float hv = heights[i];
      if (hv < skip_below) {
        continue;
      }
      zmin = (std::min)(zmin, hv);
      zmax = (std::max)(zmax, hv);
    }
  }
  if (!(zmax > zmin)) {
    return false;
  }
  *zmin_out = zmin;
  *zmax_out = zmax;
  return true;
}

bool contour_level_bounds(float zmin, float zmax, float interval_m,
                          float* interval_out, float* z0_out) {
  if (!interval_out || !z0_out) {
    return false;
  }
  const float interval =
      interval_m > 0.f ? interval_m : pick_contour_interval_m(zmin, zmax);
  if (!(interval > 0.f)) {
    return false;
  }
  float z0 = std::ceil(zmin / interval) * interval;
  if (z0 < zmin + 0.25f * interval) {
    z0 += interval;
  }
  if (!(z0 < zmax - 0.15f * interval)) {
    return false;
  }
  *interval_out = interval;
  *z0_out = z0;
  return true;
}

void collect_isoline_xy(const float* heights, int cols, int rows, float z,
                        float skip_below, int chaikin_iters,
                        std::vector<float>* segs) {
  if (!heights || !segs || cols < 2 || rows < 2) {
    return;
  }
  segs->clear();
  segs->reserve(static_cast<std::size_t>(cols) * 8u);
  for (int y = 0; y < rows - 1; ++y) {
    for (int x = 0; x < cols - 1; ++x) {
      const std::size_t i00 =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
          static_cast<std::size_t>(x);
      const float h00 = heights[i00];
      const float h10 = heights[i00 + 1];
      const float h01 = heights[i00 + static_cast<std::size_t>(cols)];
      const float h11 = heights[i00 + static_cast<std::size_t>(cols) + 1];
      if (h00 < skip_below && h10 < skip_below && h01 < skip_below &&
          h11 < skip_below) {
        continue;
      }
      cell_segments(h00, h10, h11, h01, z, static_cast<float>(x),
                    static_cast<float>(y), segs);
    }
  }
  smooth_isoline_xy_segments(segs, chaikin_iters);
}

}  // namespace detail
}  // namespace vista

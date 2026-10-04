// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/hillshade.h"

#include <algorithm>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"

namespace gis {
namespace detail {
namespace {

constexpr int kParallelShadeMinRows = 8;
constexpr int kParallelShadeMinPixels = 4096;

float sample_elev(const float* heights, int cols, int rows, int col, int row) {
  col = (std::max)(0, (std::min)(cols - 1, col));
  row = (std::max)(0, (std::min)(rows - 1, row));
  return heights[static_cast<size_t>(row) * static_cast<size_t>(cols) +
                 static_cast<size_t>(col)];
}

}  // namespace

bool horn_lambert_shade_grid(const float* heights, int cols, int rows,
                             int step_x, int step_y, float dx_m, float dy_m,
                             float exaggeration, float azimuth_rad,
                             float sin_altitude, float cos_altitude,
                             std::vector<float>* shade_out, int* out_w,
                             int* out_h) {
  if (!heights || !shade_out || cols < 2 || rows < 2) {
    return false;
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols + step_x - 1) / step_x;
  const int h = (rows + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }
  shade_out->assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0.f);
  float* shade = shade_out->data();

  auto fill_row = [&](int row) {
    const int src_row = (std::min)(rows - 1, row * step_y);
    float* rowp = shade + static_cast<size_t>(row) * static_cast<size_t>(w);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols - 1, col * step_x);
      const float west = sample_elev(heights, cols, rows, src_col - step_x, src_row);
      const float east = sample_elev(heights, cols, rows, src_col + step_x, src_row);
      const float south =
          sample_elev(heights, cols, rows, src_col, src_row + step_y);
      const float north =
          sample_elev(heights, cols, rows, src_col, src_row - step_y);
      rowp[col] = horn_lambert_shade(west, east, south, north, dx_m, dy_m,
                                     exaggeration, azimuth_rad, sin_altitude,
                                     cos_altitude);
    }
  };

  if (h >= kParallelShadeMinRows && w * h >= kParallelShadeMinPixels) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, h, fill_row);
  } else {
    for (int row = 0; row < h; ++row) {
      fill_row(row);
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  return true;
}

}  // namespace detail
}  // namespace gis

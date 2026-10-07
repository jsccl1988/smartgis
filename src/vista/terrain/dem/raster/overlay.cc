// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "vista/terrain/dem/dem_contour.h"
#include "vista/terrain/dem/raster/bake_util.h"

namespace vista {

bool DemRaster::bake_elevation_overlay_rgba(int max_edge, bool surface,
                                            bool curves,
                                            std::vector<uint8_t>* rgba,
                                            int* out_w, int* out_h) const {
  if (!rgba || empty() || (!surface && !curves)) {
    return false;
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols_ > max_edge) {
      step_x = cols_ / max_edge;
    }
    if (rows_ > max_edge) {
      step_y = rows_ / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols_ + step_x - 1) / step_x;
  const int h = (rows_ + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }
  std::vector<float> grid(static_cast<size_t>(w) * static_cast<size_t>(h), 0.f);
  float* gp = grid.data();
  auto lod_row = [&](int row) {
    const int src_row = (std::min)(rows_ - 1, row * step_y);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols_ - 1, col * step_x);
      float m = meters_at(src_col, src_row);
      if (!land_.empty() &&
          land_[static_cast<size_t>(index_at(src_col, src_row))] == 0) {
        m = 0.f;
      }
      gp[static_cast<size_t>(row) * static_cast<size_t>(w) +
         static_cast<size_t>(col)] = m;
    }
  };
  detail::for_each_bake_row(w, h, lod_row);
  if (!vista::bake_elevation_overlay_rgba(grid.data(), w, h, surface, curves,
                                          0.f, rgba)) {
    return false;
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  return true;
}

}  // namespace vista

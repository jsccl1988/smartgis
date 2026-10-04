// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/mask/ring_mask.h"

#include <algorithm>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"

namespace gis {
namespace detail {
namespace {

constexpr int kParallelMaskMinRows = 8;
constexpr int kParallelMaskMinCells = 4096;

bool ring_bbox_misses(const RingMaskBBox& box, double px, double py) {
  if (!box.valid) {
    return true;
  }
  return px < box.minx || px > box.maxx || py < box.miny || py > box.maxy;
}

}  // namespace

void fill_ring_mask(double minx, double miny, double maxx, double maxy, int cols,
                    int rows, const double* ring_x, const double* ring_y,
                    const int* ring_off, int ring_count, const RingMaskBBox* bboxes,
                    uint8_t* out) {
  if (!out || cols < 1 || rows < 1) {
    return;
  }
  const double dx =
      (maxx - minx) / static_cast<double>((std::max)(1, cols - 1));
  const double dy =
      (maxy - miny) / static_cast<double>((std::max)(1, rows - 1));

  auto fill_row = [&](int row) {
    const double lat = maxy - row * dy;
    uint8_t* rowp = out + static_cast<size_t>(row) * static_cast<size_t>(cols);
    for (int col = 0; col < cols; ++col) {
      const double lon = minx + col * dx;
      uint8_t hit = 0;
      for (int r = 0; r < ring_count; ++r) {
        if (bboxes && ring_bbox_misses(bboxes[r], lon, lat)) {
          continue;
        }
        if (!ring_x || !ring_y || !ring_off) {
          continue;
        }
        const int a = ring_off[r];
        const int b = ring_off[r + 1];
        const int n = b - a;
        if (n < 3) {
          continue;
        }
        if (point_in_ring(lon, lat, ring_x + a, ring_y + a,
                          static_cast<size_t>(n))) {
          hit = 1;
          break;
        }
      }
      rowp[col] = hit;
    }
  };

  const int cells = cols * rows;
  if (rows >= kParallelMaskMinRows && cells >= kParallelMaskMinCells) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, rows, fill_row);
  } else {
    for (int row = 0; row < rows; ++row) {
      fill_row(row);
    }
  }
}

}  // namespace detail
}  // namespace gis

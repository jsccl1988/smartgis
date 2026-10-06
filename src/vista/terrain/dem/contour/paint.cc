// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/contour/paint.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vista {
namespace detail {
namespace {

void put_px(std::vector<uint8_t>* rgba, int cols, int rows, int x, int y,
            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (!rgba || x < 0 || y < 0 || x >= cols || y >= rows) {
    return;
  }
  const std::size_t i =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
       static_cast<std::size_t>(x)) *
      4u;
  (*rgba)[i + 0] = r;
  (*rgba)[i + 1] = g;
  (*rgba)[i + 2] = b;
  (*rgba)[i + 3] = a;
}

void stamp_px(std::vector<uint8_t>* rgba, int cols, int rows, int x, int y,
              bool thick) {
  // White isolines (Origin stacked-surface look) on the jet sheet.
  put_px(rgba, cols, rows, x, y, 248, 252, 255, 255);
  if (!thick) {
    return;
  }
  put_px(rgba, cols, rows, x + 1, y, 248, 252, 255, 255);
  put_px(rgba, cols, rows, x, y + 1, 248, 252, 255, 255);
}

void stroke_seg(std::vector<uint8_t>* rgba, int cols, int rows, float x0,
                float y0, float x1, float y1, bool thick) {
  int ix0 = static_cast<int>(std::lround(x0));
  int iy0 = static_cast<int>(std::lround(y0));
  int ix1 = static_cast<int>(std::lround(x1));
  int iy1 = static_cast<int>(std::lround(y1));
  int dx = std::abs(ix1 - ix0);
  int dy = std::abs(iy1 - iy0);
  const int sx = ix0 < ix1 ? 1 : -1;
  const int sy = iy0 < iy1 ? 1 : -1;
  int err = dx - dy;
  for (;;) {
    stamp_px(rgba, cols, rows, ix0, iy0, thick);
    if (ix0 == ix1 && iy0 == iy1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      ix0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      iy0 += sy;
    }
  }
}

}  // namespace

void stroke_isoline_xy(std::vector<uint8_t>* rgba, int cols, int rows,
                       const std::vector<float>& segs, bool thick) {
  if (!rgba) {
    return;
  }
  for (std::size_t i = 0; i + 3u < segs.size(); i += 4u) {
    stroke_seg(rgba, cols, rows, segs[i], segs[i + 1u], segs[i + 2u],
               segs[i + 3u], thick);
  }
}

}  // namespace detail
}  // namespace vista

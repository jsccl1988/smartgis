// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CONTOUR_PAINT_H_
#define VISTA_TERRAIN_DEM_CONTOUR_PAINT_H_

#include <cstdint>
#include <vector>

namespace vista {
namespace detail {

// Bresenham-stroke isoline segments (x0,y0,x1,y1…) onto an RGBA8 grid.
// Dark charcoal marks (not Origin white). |thick| widens to 2×2 only when
// the bake grid is large enough that coverage stays sparse.
void stroke_isoline_xy(std::vector<uint8_t>* rgba, int cols, int rows,
                       const std::vector<float>& segs, bool thick);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CONTOUR_PAINT_H_

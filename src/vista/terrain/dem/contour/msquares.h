// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CONTOUR_MSQUARES_H_
#define VISTA_TERRAIN_DEM_CONTOUR_MSQUARES_H_

#include <vector>

namespace vista {
namespace detail {

// Marching-squares segment for one DEM cell at isolevel |z|.
// Corners: h00 SW, h10 SE, h11 NE, h01 NW. Appends x0,y0,x1,y1 in grid units
// (|c|,|r| = SW corner column/row).
void cell_segments(float h00, float h10, float h11, float h01, float z,
                   float c, float r, std::vector<float>* xy);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CONTOUR_MSQUARES_H_

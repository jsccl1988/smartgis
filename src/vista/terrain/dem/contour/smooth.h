// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CONTOUR_SMOOTH_H_
#define VISTA_TERRAIN_DEM_CONTOUR_SMOOTH_H_

#include <vector>

namespace vista {
namespace detail {

// Default Chaikin passes after chaining cell segments into polylines.
inline constexpr int kContourChaikinIters = 2;

// Chain unordered marching-squares segments (x0,y0,x1,y1…) into polylines,
// then Chaikin-smooth so isolines stop looking like DEM cell stairsteps.
void smooth_isoline_xy_segments(std::vector<float>* segs, int chaikin_iters);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CONTOUR_SMOOTH_H_

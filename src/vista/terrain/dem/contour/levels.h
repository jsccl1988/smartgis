// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CONTOUR_LEVELS_H_
#define VISTA_TERRAIN_DEM_CONTOUR_LEVELS_H_

#include <vector>

namespace vista {
namespace detail {

// Heights below this are treated as ocean / skip for DEM overlays.
inline constexpr float kDemOceanMaxM = 1.f;

// Min/max over land cells (h >= skip_below). Parallel when the grid is large.
bool land_height_range(const float* heights, int cols, int rows,
                       float skip_below, float* zmin_out, float* zmax_out);

// Resolve |interval_m| (<=0 → auto) and first drawable level |z0|.
// Returns false when no isolines can be drawn.
bool contour_level_bounds(float zmin, float zmax, float interval_m,
                          float* interval_out, float* z0_out);

// Marching-squares over the grid at isolevel |z|, then Chaikin-smooth.
// Cells with all four corners < |skip_below| are skipped.
void collect_isoline_xy(const float* heights, int cols, int rows, float z,
                        float skip_below, int chaikin_iters,
                        std::vector<float>* segs);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CONTOUR_LEVELS_H_

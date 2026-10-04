// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ANALYSIS_RASTER_DEM_HILLSHADE_H_
#define GIS_ANALYSIS_RASTER_DEM_HILLSHADE_H_

#include <algorithm>
#include <cmath>
#include <vector>

#include "gis/gis_export.h"

namespace gis {
namespace detail {

// Horn central-difference Lambert factor in [0, 1], before paint contrast.
// Neighbor samples are already clamped. |dx_m| / |dy_m| are the full
// east-west and north-south spans (typically 2 * step * cell size).
// Zero spans are treated as 1 so a flat cell stays sin(altitude).
inline float horn_lambert_shade(float west_m, float east_m, float south_m,
                                float north_m, float dx_m, float dy_m,
                                float exaggeration, float azimuth_rad,
                                float sin_altitude, float cos_altitude) {
  const float dx = dx_m == 0.f ? 1.f : dx_m;
  const float dy = dy_m == 0.f ? 1.f : dy_m;
  const float sx = -((east_m - west_m) / dx) * exaggeration;
  const float sy = -((north_m - south_m) / dy) * exaggeration;
  const float slope = std::atan(std::sqrt(sx * sx + sy * sy));
  float aspect = 0.f;
  if (sx != 0.f || sy != 0.f) {
    aspect = std::atan2(sy, -sx);
  }
  const float shade = sin_altitude * std::cos(slope) +
                      cos_altitude * std::sin(slope) *
                          std::cos(azimuth_rad - aspect);
  return std::clamp(shade, 0.f, 1.f);
}

// Downsampled Horn shade, one factor per output pixel (row-major).
// |heights| is row-major |cols| * |rows|; samples clamp to the grid.
// Row 0 is north. Returns false when the stepped grid is smaller than 2×2
// or |heights| is null. Does not apply ocean alpha or paint contrast.
GIS_EXPORT bool horn_lambert_shade_grid(const float* heights, int cols,
                                       int rows, int step_x, int step_y,
                                       float dx_m, float dy_m,
                                       float exaggeration, float azimuth_rad,
                                       float sin_altitude, float cos_altitude,
                                       std::vector<float>* shade_out, int* out_w,
                                       int* out_h);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_RASTER_DEM_HILLSHADE_H_

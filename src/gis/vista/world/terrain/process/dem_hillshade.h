// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_DEM_HILLSHADE_H_
#define GIS_WORLD_DEM_HILLSHADE_H_

#include <cstdint>
#include <vector>

#include "gis/gis_export.h"
#include "gis/vista/world/terrain/dem/dem_raster.h"

namespace gis {

// Parameters for owned DEM hillshade (Style Spec paint names, own math).
struct HillshadeParams {
  float illumination_direction_deg = 335.f;  // 0 = north, clockwise
  float illumination_altitude_deg = 45.f;
  float exaggeration = 0.5f;
  uint32_t shadow_argb = 0xFF000000u;
  uint32_t highlight_argb = 0xFFFFFFFFu;
  uint32_t accent_argb = 0xFF000000u;
  int max_edge = 256;
};

// Finite-difference slope/aspect shade → tightly packed RGBA8.
// Nodata / ocean cells get alpha 0. Returns false when DEM empty or too small.
GIS_EXPORT bool shade_dem_rgba(const DemRaster& dem,
                               const HillshadeParams& params,
                               std::vector<uint8_t>* rgba, int* out_w,
                               int* out_h);

}  // namespace gis

#endif  // GIS_WORLD_DEM_HILLSHADE_H_

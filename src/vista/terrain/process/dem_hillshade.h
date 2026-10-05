// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_PROCESS_DEM_HILLSHADE_H_
#define VISTA_TERRAIN_PROCESS_DEM_HILLSHADE_H_

#include <cstdint>
#include <vector>

#include "vista/vista_export.h"
#include "vista/terrain/dem/dem_raster.h"

namespace vista {

// Parameters for owned DEM hillshade (Style Spec paint names, own math).
struct HillshadeParams {
  float illumination_direction_deg = 335.f;  // 0 = north, clockwise
  // Lower altitude → longer, sharper umbra on steep DEM faces.
  float illumination_altitude_deg = 32.f;
  float exaggeration = 0.5f;
  uint32_t shadow_argb = 0xFF000000u;
  uint32_t highlight_argb = 0xFFFFFFFFu;
  uint32_t accent_argb = 0xFF000000u;
  int max_edge = 256;
};

// Finite-difference slope/aspect shade → tightly packed RGBA8.
// Nodata / ocean cells get alpha 0. Returns false when DEM empty or too small.
VISTA_EXPORT bool shade_dem_rgba(const DemRaster& dem,
                               const HillshadeParams& params,
                               std::vector<uint8_t>* rgba, int* out_w,
                               int* out_h);

// Last shade_dem_rgba backend: 1 if try_shade_dem_thrust won.
VISTA_EXPORT int last_shade_used_cuda();
VISTA_EXPORT void reset_last_shade_used_cuda();

}  // namespace vista

#endif  // VISTA_TERRAIN_PROCESS_DEM_HILLSHADE_H_

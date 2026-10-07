// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#include "vista/terrain/dem/nv/bake_pixel.h"

namespace vista {

// Base elevation ramp. Lowlands stay green (landish gate: g>r and g>b).
// Highlands are tan, not a pink-white poster. Slope/snow live in the bake.
void hypsometric_rgb(float meters, float* r, float* g, float* b) {
  detail::hypsometric_rgb_impl(meters, r, g, b);
}

void terrain_material_rgb(float meters, float slope01, float* r, float* g,
                          float* b) {
  detail::terrain_material_rgb_impl(meters, slope01, r, g, b);
}

}  // namespace vista

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/nv/thrust_gis.h"

namespace vista {

bool try_fill_lonlat_mask_thrust(double, double, double, double, int, int,
                                 const double*, const double*, const int*, int,
                                 int, uint8_t*) {
  return false;
}

bool try_shade_dem_thrust(const float*, int, int, int, int, int, int, float,
                          float, float, float, float, float, float, float,
                          float, float, float, float, uint8_t*) {
  return false;
}

bool try_bake_hypso_thrust(const float*, const uint8_t*, int, int, int, int,
                           int, int, uint8_t*) {
  return false;
}

bool try_jet_fill_thrust(const float*, int, int, float, float, uint8_t*) {
  return false;
}

bool thrust_gis_cuda_built() {
  return false;
}

}  // namespace vista

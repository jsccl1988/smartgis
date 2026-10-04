// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_tile_math.h"

#include <algorithm>
#include <cmath>

#include "gis/carto/tile/xyz_math.h"

namespace content {
namespace detail {

constexpr double kPi = 3.14159265358979323846;

double lon_to_merc_x(double lon) {
  return lon * gis::tile::k_web_mercator_half / 180.0;
}

double lat_to_merc_y(double lat) {
  const double clamped =
      (std::max)(-85.05112878, (std::min)(85.05112878, lat));
  const double rad = clamped * kPi / 180.0;
  return std::log(std::tan(kPi / 4.0 + rad / 2.0)) *
         gis::tile::k_web_mercator_half / kPi;
}

double merc_x_to_lon(double x) {
  return x * 180.0 / gis::tile::k_web_mercator_half;
}

double merc_y_to_lat(double y) {
  const double rad =
      2.0 * (std::atan(std::exp(y * kPi / gis::tile::k_web_mercator_half)) -
             kPi / 4.0);
  return rad * 180.0 / kPi;
}

double zoom_from_scale(double scale) {
  const double z = 8.0 + std::log2((std::max)(scale, 1e-3));
  if (z < 0.0) {
    return 0.0;
  }
  if (z > 22.0) {
    return 22.0;
  }
  return z;
}

}  // namespace detail
}  // namespace content

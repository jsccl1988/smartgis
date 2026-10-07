// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/protocol/mercator_math.h"

#include <algorithm>
#include <cmath>

#include "gis/tile/protocol/xyz_math.h"

namespace gis {
namespace tile {
namespace {

constexpr double k_pi = 3.14159265358979323846;

}  // namespace

double lon_to_merc_x(double lon) {
  return lon * k_web_mercator_half / 180.0;
}

double lat_to_merc_y(double lat) {
  const double clamped =
      (std::max)(-85.05112878, (std::min)(85.05112878, lat));
  const double rad = clamped * k_pi / 180.0;
  return std::log(std::tan(k_pi / 4.0 + rad / 2.0)) * k_web_mercator_half /
         k_pi;
}

double merc_x_to_lon(double x) {
  return x * 180.0 / k_web_mercator_half;
}

double merc_y_to_lat(double y) {
  const double rad =
      2.0 * (std::atan(std::exp(y * k_pi / k_web_mercator_half)) - k_pi / 4.0);
  return rad * 180.0 / k_pi;
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

}  // namespace tile
}  // namespace gis

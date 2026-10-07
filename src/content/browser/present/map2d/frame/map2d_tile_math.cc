// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scheduler over vista tile math. Signatures stay for frame cache and
// software paint.

#include "content/browser/present/map2d/frame/map2d_tile_math.h"

#include "vista/component/map/tile/math.h"

namespace content {
namespace detail {

double lon_to_merc_x(double lon) {
  return vista::lon_to_merc_x(lon);
}

double lat_to_merc_y(double lat) {
  return vista::lat_to_merc_y(lat);
}

double merc_x_to_lon(double x) {
  return vista::merc_x_to_lon(x);
}

double merc_y_to_lat(double y) {
  return vista::merc_y_to_lat(y);
}

double zoom_from_scale(double scale) {
  return vista::zoom_from_scale(scale);
}

}  // namespace detail
}  // namespace content

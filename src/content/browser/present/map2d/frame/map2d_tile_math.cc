// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Thin present forwarder over gis::tile Mercator helpers.

#include "content/browser/present/map2d/frame/map2d_tile_math.h"

#include "gis/tile/protocol/mercator_math.h"

namespace content {
namespace detail {

double lon_to_merc_x(double lon) {
  return gis::tile::lon_to_merc_x(lon);
}

double lat_to_merc_y(double lat) {
  return gis::tile::lat_to_merc_y(lat);
}

double merc_x_to_lon(double x) {
  return gis::tile::merc_x_to_lon(x);
}

double merc_y_to_lat(double y) {
  return gis::tile::merc_y_to_lat(y);
}

double zoom_from_scale(double scale) {
  return gis::tile::zoom_from_scale(scale);
}

}  // namespace detail
}  // namespace content

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Web Mercator meters and overlay-scale zoom. Degrees in, no GisScene.

#ifndef VISTA_COMPONENT_MAP_TILE_MATH_H_
#define VISTA_COMPONENT_MAP_TILE_MATH_H_

#include "vista/vista_export.h"

namespace vista {

VISTA_EXPORT double lon_to_merc_x(double lon);
VISTA_EXPORT double lat_to_merc_y(double lat);
VISTA_EXPORT double merc_x_to_lon(double x);
VISTA_EXPORT double merc_y_to_lat(double y);

// MapLibre-ish zoom from overlay scale (px per map unit / degree).
VISTA_EXPORT double zoom_from_scale(double scale);

}  // namespace vista

#endif  // VISTA_COMPONENT_MAP_TILE_MATH_H_

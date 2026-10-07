// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Web Mercator meters and overlay-scale zoom. Degrees in; no GisScene.

#ifndef GIS_TILE_MERCATOR_MATH_H_
#define GIS_TILE_MERCATOR_MATH_H_

#include "gis/gis_export.h"

namespace gis {
namespace tile {

GIS_EXPORT double lon_to_merc_x(double lon);
GIS_EXPORT double lat_to_merc_y(double lat);
GIS_EXPORT double merc_x_to_lon(double x);
GIS_EXPORT double merc_y_to_lat(double y);

// MapLibre-ish zoom from overlay scale (px per map unit / degree).
GIS_EXPORT double zoom_from_scale(double scale);

}  // namespace tile
}  // namespace gis

#endif  // GIS_TILE_MERCATOR_MATH_H_

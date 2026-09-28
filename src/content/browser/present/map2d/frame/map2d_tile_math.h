// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_TILE_MATH_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_TILE_MATH_H_

namespace content {
namespace detail {

double lon_to_merc_x(double lon);
double lat_to_merc_y(double lat);
double merc_x_to_lon(double x);
double merc_y_to_lat(double y);
// MapLibre-ish zoom from overlay scale (px per map unit / degree).
double zoom_from_scale(double scale);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_TILE_MATH_H_

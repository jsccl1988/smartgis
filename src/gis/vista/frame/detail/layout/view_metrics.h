// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// View-to-screen metrics used by Layout::build and symbol placement.

#ifndef GIS_VISTA_DETAIL_LAYOUT_VIEW_METRICS_H_
#define GIS_VISTA_DETAIL_LAYOUT_VIEW_METRICS_H_

#include "gis/vista/frame/frame.h"

namespace gis {
namespace vista {
namespace detail {

double world_units_per_pixel(const View& view);
float device_px_per_world(const View& view);
bool screen_ready(const View& view);

// Screen pixel from world lon/lat. North is up in world y and down in pixel y.
struct ScreenPt {
  float x = 0;
  float y = 0;
};

ScreenPt to_screen(const View& view, double x, double y);

}  // namespace detail
}  // namespace vista
}  // namespace gis

#endif  // GIS_VISTA_DETAIL_LAYOUT_VIEW_METRICS_H_

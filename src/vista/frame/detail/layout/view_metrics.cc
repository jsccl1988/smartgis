// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/frame/detail/layout/view_metrics.h"

namespace vista {
namespace detail {

double world_units_per_pixel(const View& view) {
  if (view.width_px == 0 || view.max_x <= view.min_x) {
    return 0;
  }
  return (view.max_x - view.min_x) / static_cast<double>(view.width_px);
}

float device_px_per_world(const View& view) {
  const double wupp = world_units_per_pixel(view);
  if (wupp <= 0) {
    return 1.f;
  }
  return static_cast<float>(1.0 / wupp);
}

bool screen_ready(const View& view) {
  return view.width_px > 0 && view.height_px > 0 && view.max_x > view.min_x &&
         view.max_y > view.min_y;
}

ScreenPt to_screen(const View& view, double x, double y) {
  ScreenPt s;
  s.x = static_cast<float>((x - view.min_x) / (view.max_x - view.min_x) *
                           static_cast<double>(view.width_px));
  s.y = static_cast<float>((view.max_y - y) / (view.max_y - view.min_y) *
                           static_cast<double>(view.height_px));
  return s;
}

}  // namespace detail
}  // namespace vista

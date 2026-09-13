// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_PROJ_PROJ_BACKEND_TRAITS_H_
#define ALGORITHM_PROJ_PROJ_BACKEND_TRAITS_H_

#include "base/core/core.h"
#include "algorithm/proj/proj_runtime.h"

#include <concepts>

namespace proj {

// Point types with x/y readable as double (dbfPoint and similar).
template <typename Point>
concept lonlat_point = requires(Point p) {
  { p.x } -> std::convertible_to<double>;
  { p.y } -> std::convertible_to<double>;
};

// PROJ 9 backend for product points. Lives in proj (not geo) so it does
// not collide with geometry-agent traits.
template <lonlat_point Point>
struct proj_backend_traits {
  static int transform(PJ* pipeline, Point& point, PJ_DIRECTION direction) {
    double x = static_cast<double>(point.x);
    double y = static_cast<double>(point.y);
    const int rc = detail::trans_xy(pipeline, direction, &x, &y);
    if (rc != SMT_ERR_NONE) {
      return rc;
    }
    point.x = static_cast<decltype(point.x)>(x);
    point.y = static_cast<decltype(point.y)>(y);
    return SMT_ERR_NONE;
  }

  static int transform(const char* source_crs,
                       const char* target_crs,
                       Point& point) {
    PJ* pipeline = detail::cached_pipeline(source_crs, target_crs);
    if (!pipeline) {
      return SMT_ERR_FAILURE;
    }
    return transform(pipeline, point, PJ_FWD);
  }
};

}  // namespace proj

#endif  // ALGORITHM_PROJ_PROJ_BACKEND_TRAITS_H_

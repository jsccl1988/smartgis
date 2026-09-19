// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_MAP_HOST_EXTENT_H_
#define APP_VIEWS_MAP_HOST_EXTENT_H_

#include "content/public/map_types.h"

namespace app {

// Leftover SmartGis default China envelope (CRS84 lon/lat). Used when
// MapContents has not published an extent yet but the host still needs a
// full-country 2D ortho / 3D framing box.
inline constexpr content::Extent2 kChinaLonLatExtent{73.0, 18.0, 135.0, 54.0};

inline bool extent_nonempty(const content::Extent2& e) {
  return e.xmax > e.xmin && e.ymax > e.ymin;
}

inline bool extent_looks_like_china(const content::Extent2& e) {
  if (!extent_nonempty(e)) {
    return false;
  }
  return e.xmin >= 60.0 && e.xmax <= 145.0 && e.ymin >= 3.0 && e.ymax <= 60.0;
}

inline content::Extent2 china_or(const content::Extent2& e) {
  return extent_nonempty(e) ? e : kChinaLonLatExtent;
}

}  // namespace app

#endif  // APP_VIEWS_MAP_HOST_EXTENT_H_

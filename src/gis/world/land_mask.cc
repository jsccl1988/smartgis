// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/world/land_mask.h"

#include <algorithm>

namespace gis {

void LonLatRing::prepare_bbox() const {
  has_bbox = false;
  if (empty()) {
    return;
  }
  minx = maxx = x[0];
  miny = maxy = y[0];
  for (size_t i = 1; i < x.size(); ++i) {
    minx = (std::min)(minx, x[i]);
    maxx = (std::max)(maxx, x[i]);
    miny = (std::min)(miny, y[i]);
    maxy = (std::max)(maxy, y[i]);
  }
  has_bbox = true;
}

bool LonLatRing::bbox_may_contain(double px, double py) const {
  if (!has_bbox) {
    prepare_bbox();
  }
  return has_bbox && px >= minx && px <= maxx && py >= miny && py <= maxy;
}

bool point_in_lonlat_ring(double px, double py, const LonLatRing& ring) {
  if (ring.empty()) {
    return false;
  }
  const size_t n = ring.x.size();
  bool inside = false;
  size_t j = n - 1;
  for (size_t i = 0; i < n; ++i) {
    const double xi = ring.x[i];
    const double yi = ring.y[i];
    const double xj = ring.x[j];
    const double yj = ring.y[j];
    const bool intersect =
        ((yi > py) != (yj > py)) &&
        (px < (xj - xi) * (py - yi) / ((yj - yi) + 0.0) + xi);
    if (intersect) {
      inside = !inside;
    }
    j = i;
  }
  return inside;
}

bool any_ring_contains(double px, double py,
                       const std::vector<LonLatRing>& rings) {
  for (const LonLatRing& ring : rings) {
    if (!ring.bbox_may_contain(px, py)) {
      continue;
    }
    if (point_in_lonlat_ring(px, py, ring)) {
      return true;
    }
  }
  return false;
}

}  // namespace gis

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_XY_XY_H_
#define GIS_GEO_XY_XY_H_

#include <vector>

#include "gis/gis_export.h"

namespace geo {

// Map-plane point (double). Not an OGR geometry; no CRS.
struct Xy {
  double x = 0;
  double y = 0;
};

// Andrew's monotone chain. Returns hull ring without repeating the first
// point. Degenerate / empty input returns ≤2 points.
GIS_EXPORT std::vector<Xy> convex_hull_xy(std::vector<Xy> pts);

// Absolute shoelace area. Ring size < 3 → 0.
GIS_EXPORT double polygon_area_xy(const std::vector<Xy>& ring);

// Axis-aligned bbox area of the point set. Empty → 0.
GIS_EXPORT double bbox_area_xy(const std::vector<Xy>& pts);

}  // namespace geo

#endif  // GIS_GEO_XY_XY_H_

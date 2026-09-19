// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_WORLD_LAND_MASK_H_
#define GIS_WORLD_LAND_MASK_H_

#include <vector>

namespace gis {

// Closed lon/lat ring used to mask a rectangular DEM into a country outline.
// prepare_bbox() lets any_ring_contains skip full even-odd tests when the
// query point is outside a ring's axis-aligned envelope (china_city scale).
struct LonLatRing {
  std::vector<double> x;
  std::vector<double> y;
  mutable double minx = 0;
  mutable double miny = 0;
  mutable double maxx = 0;
  mutable double maxy = 0;
  mutable bool has_bbox = false;
  bool empty() const { return x.size() < 3 || x.size() != y.size(); }
  void prepare_bbox() const;
  bool bbox_may_contain(double px, double py) const;
};

// Even-odd point-in-polygon. Ring is closed or open (last≠first is OK).
bool point_in_lonlat_ring(double px, double py, const LonLatRing& ring);

bool any_ring_contains(double px, double py,
                       const std::vector<LonLatRing>& rings);

}  // namespace gis

#endif  // GIS_WORLD_LAND_MASK_H_

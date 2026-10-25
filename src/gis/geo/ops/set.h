// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_SET_H_
#define GIS_GEO_OPS_SET_H_

#include "gis/gis_export.h"
#include "gis/geo/ops/geometry_traits.h"

class OGRGeometry;

namespace geo {

enum class SetOp { kIntersection, kUnion, kDifference, kSymDifference };

// Binary set ops via geos_c. Caller owns the result. Z is not sent through
// GEOS; restore instance dimension on the result when needed.
GIS_EXPORT OGRGeometry* set_via_geos(const OGRGeometry& a,
                                     const OGRGeometry& b,
                                     SetOp op);

template <ogr_geometry_like G>
OGRGeometry* intersection(const G& a, const G& b) {
  return set_via_geos(a, b, SetOp::kIntersection);
}

template <ogr_geometry_like G>
OGRGeometry* geom_union(const G& a, const G& b) {
  return set_via_geos(a, b, SetOp::kUnion);
}

template <ogr_geometry_like G>
OGRGeometry* difference(const G& a, const G& b) {
  return set_via_geos(a, b, SetOp::kDifference);
}

template <ogr_geometry_like G>
OGRGeometry* sym_difference(const G& a, const G& b) {
  return set_via_geos(a, b, SetOp::kSymDifference);
}

}  // namespace geo

#endif  // GIS_GEO_OPS_SET_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_PREDICATE_H_
#define GIS_GEO_OPS_PREDICATE_H_

#include "gis/geo/ops/geometry_traits.h"

#include "ogr_geometry.h"

namespace geo {

enum class SpatialPredicate {
  kIntersects,
  kContains,
  kWithin,
  kDisjoint,
  kEquals,
  kTouches,
  kCrosses,
  kOverlaps,
  kCovers,
  kCoveredBy,
};

// OGR spatial predicates. Empty × empty: only Disjoint is true.
template <ogr_geometry_like G>
bool predicate_holds(SpatialPredicate op, const G& left, const G& right) {
  if (left.IsEmpty() || right.IsEmpty()) {
    return op == SpatialPredicate::kDisjoint;
  }
  switch (op) {
    case SpatialPredicate::kIntersects:
      return left.Intersects(&right) != 0;
    case SpatialPredicate::kContains:
      return left.Contains(&right) != 0;
    case SpatialPredicate::kWithin:
      return left.Within(&right) != 0;
    case SpatialPredicate::kDisjoint:
      return left.Disjoint(&right) != 0;
    case SpatialPredicate::kEquals:
      return left.Equals(&right) != 0;
    case SpatialPredicate::kTouches:
      return left.Touches(&right) != 0;
    case SpatialPredicate::kCrosses:
      return left.Crosses(&right) != 0;
    case SpatialPredicate::kOverlaps:
      return left.Overlaps(&right) != 0;
    case SpatialPredicate::kCovers:
      // OGRGeometry has no Covers on this GDAL; Contains is the closest.
      return left.Contains(&right) != 0;
    case SpatialPredicate::kCoveredBy:
      return left.Within(&right) != 0;
  }
  return false;
}

template <ogr_geometry_like G>
bool intersects(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kIntersects, a, b);
}

template <ogr_geometry_like G>
bool contains(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kContains, a, b);
}

template <ogr_geometry_like G>
bool within(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kWithin, a, b);
}

template <ogr_geometry_like G>
bool disjoint(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kDisjoint, a, b);
}

template <ogr_geometry_like G>
bool equals(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kEquals, a, b);
}

template <ogr_geometry_like G>
bool touches(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kTouches, a, b);
}

template <ogr_geometry_like G>
bool crosses(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kCrosses, a, b);
}

template <ogr_geometry_like G>
bool overlaps(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kOverlaps, a, b);
}

template <ogr_geometry_like G>
bool covers(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kCovers, a, b);
}

template <ogr_geometry_like G>
bool covered_by(const G& a, const G& b) {
  return predicate_holds(SpatialPredicate::kCoveredBy, a, b);
}

}  // namespace geo

#endif  // GIS_GEO_OPS_PREDICATE_H_

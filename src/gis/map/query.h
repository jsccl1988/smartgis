// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MAP_QUERY_H_
#define GIS_MAP_QUERY_H_

class OGRGeometry;

namespace gis {

enum class SpatialRelation : unsigned {
  kUnknown = 0,
  kWithin = 1,
  kTouches = 1u << 1,
  kCrosses = 1u << 2,
  kOverlaps = 1u << 3,
  kIntersects = 1u << 4,
  kEquals = 1u << 5,
  kContains = 1u << 6,
  kDisjoint = 1u << 7,
};

struct GeomQueryDesc {
  OGRGeometry* geometry = nullptr;
  SpatialRelation spatial_relation = SpatialRelation::kContains;
  float margin = 0.05f;
};

struct AttrQueryDesc {
  char** field_names = nullptr;
  char** field_queries = nullptr;
};

}  // namespace gis

#endif  // GIS_MAP_QUERY_H_

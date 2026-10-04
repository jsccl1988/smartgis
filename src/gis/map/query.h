// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MAP_QUERY_H_
#define GIS_MAP_QUERY_H_

class OGRGeometry;

namespace geo {

enum SpatialRelation {
  SS_Unknown = 0,
  SS_Within = 1,
  SS_Touches = 1 << 1,
  SS_Crosses = 1 << 2,
  SS_Overlaps = 1 << 3,
  SS_Intersects = 1 << 4,
  SS_Equals = 1 << 5,
  SS_Contains = 1 << 6,
  SS_Disjoint = 1 << 7
};

}  // namespace geo

namespace gis {

struct GeomQueryDesc {
  OGRGeometry* pQueryGeom;
  geo::SpatialRelation sSRs;
  float fSmargin;

  GeomQueryDesc()
      : pQueryGeom(nullptr), sSRs(geo::SS_Contains), fSmargin(0.05f) {}
};

struct AttrQueryDesc {
  char** szFldName;
  char** szFldQueryContent;

  AttrQueryDesc() : szFldName(nullptr), szFldQueryContent(nullptr) {}
};

}  // namespace gis

#endif  // GIS_MAP_QUERY_H_

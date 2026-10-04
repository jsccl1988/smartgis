// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_TIN_DELAUNAY_H_
#define GIS_GEO_TIN_DELAUNAY_H_

#include "gis/gis_export.h"
#include "base/math/vector.h"
#include "gis/geo/ops/indexed_tin.h"

#include "ogr_geometry.h"

#include <vector>

namespace geo {

// 2D Delaunay via geos_c (GEOSDelaunayTriangulation_r). Z is copied onto OGR
// TIN patches; GEOS only sees XY. Returns false on invalid input / GEOS fail.
// Indexed corners use geo::IndexedTriangle (same type as indexed_tin writes).
GIS_EXPORT bool delaunay(OGRTriangulatedSurface* mesh,
                         const base::Vector3* points,
                         int count);
GIS_EXPORT bool delaunay(OGRTriangulatedSurface* mesh,
                         const std::vector<base::Vector3>& points);

GIS_EXPORT bool delaunay_triangles(std::vector<IndexedTriangle>& triangles,
                                   const base::Vector3* points,
                                   int count);

// Constrained Delaunay of a ring (GEOSConstrainedDelaunayTriangulation_r).
GIS_EXPORT bool delaunay_constrained(std::vector<IndexedTriangle>& triangles,
                                     const base::Vector3* points,
                                     int count);

}  // namespace geo

#endif  // GIS_GEO_TIN_DELAUNAY_H_

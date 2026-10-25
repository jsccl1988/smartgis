// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_MEASURE_H_
#define GIS_GEO_OPS_MEASURE_H_

#include <string>

#include "gis/gis_export.h"

class OGRGeometry;

namespace geo {

GIS_EXPORT double distance(const OGRGeometry& a, const OGRGeometry& b);

GIS_EXPORT double area(const OGRGeometry& geom);

GIS_EXPORT double length(const OGRGeometry& geom);

GIS_EXPORT bool is_empty(const OGRGeometry& geom);

// OGR validity (typically GEOS-backed in GDAL builds).
GIS_EXPORT bool is_valid(const OGRGeometry& geom);

GIS_EXPORT bool is_simple(const OGRGeometry& geom);

// GEOS distances; returns false on failure (out unchanged).
GIS_EXPORT bool hausdorff_distance(const OGRGeometry& a,
                                   const OGRGeometry& b,
                                   double* out);

GIS_EXPORT bool frechet_distance(const OGRGeometry& a,
                                 const OGRGeometry& b,
                                 double* out);

// DE-9IM matrix string; empty on failure.
GIS_EXPORT std::string relate_de9im(const OGRGeometry& a, const OGRGeometry& b);

}  // namespace geo

#endif  // GIS_GEO_OPS_MEASURE_H_

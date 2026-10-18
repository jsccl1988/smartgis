// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// GEOS set and shape kernels. Callers own the returned OGR geometry.

#ifndef GIS_ANALYSIS_OPS_OPS_GEOS_H_
#define GIS_ANALYSIS_OPS_OPS_GEOS_H_

class OGRGeometry;

namespace gis {
namespace detail {

enum class GeosBinary { kIntersection, kUnion, kDifference, kSymDifference };

OGRGeometry* envelope_polygon(const OGRGeometry& geom);

OGRGeometry* geos_binary(const OGRGeometry& a, const OGRGeometry& b, GeosBinary op);

OGRGeometry* geos_simplify(const OGRGeometry& geom, double tolerance);

OGRGeometry* geos_convex_hull(const OGRGeometry& geom);

OGRGeometry* geos_boundary(const OGRGeometry& geom);

OGRGeometry* geos_centroid(const OGRGeometry& geom);

}  // namespace detail
}  // namespace gis

#endif  // GIS_ANALYSIS_OPS_OPS_GEOS_H_

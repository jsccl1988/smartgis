// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_SHAPE_H_
#define GIS_GEO_OPS_SHAPE_H_

#include "gis/gis_export.h"
#include "gis/geo/ops/geometry_traits.h"

class OGRGeometry;

namespace geo {

// Axis-aligned envelope as a polygon (OGR envelope, not GEOS).
GIS_EXPORT OGRGeometry* envelope_polygon(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* simplify_via_geos(const OGRGeometry& geom, double tolerance);

GIS_EXPORT OGRGeometry* convex_hull_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* boundary_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* centroid_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* make_valid_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* point_on_surface_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* densify_via_geos(const OGRGeometry& geom, double tolerance);

GIS_EXPORT OGRGeometry* offset_curve_via_geos(const OGRGeometry& geom, double width);

// ratio in [0,1]; 0 ≈ convex. allow_holes reserved (always false for now).
GIS_EXPORT OGRGeometry* concave_hull_via_geos(const OGRGeometry& geom, double ratio);

GIS_EXPORT OGRGeometry* minimum_rotated_rectangle_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* minimum_bounding_circle_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* unary_union_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* line_merge_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* node_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* polygonize_via_geos(const OGRGeometry& geom);

GIS_EXPORT OGRGeometry* remove_repeated_points_via_geos(const OGRGeometry& geom,
                                                       double tolerance);

GIS_EXPORT OGRGeometry* snap_via_geos(const OGRGeometry& geom,
                                      const OGRGeometry& reference,
                                      double tolerance);

GIS_EXPORT OGRGeometry* delaunay_via_geos(const OGRGeometry& geom, double tolerance);

GIS_EXPORT OGRGeometry* voronoi_via_geos(const OGRGeometry& geom, double tolerance);

// LineString between nearest points of a and b (caller owns).
GIS_EXPORT OGRGeometry* shortest_line_via_geos(const OGRGeometry& a, const OGRGeometry& b);

template <ogr_geometry_like G>
OGRGeometry* simplify(const G& g, typename geometry_traits<G>::coordinate_type tolerance) {
  return simplify_via_geos(g, static_cast<double>(tolerance));
}

template <ogr_geometry_like G>
OGRGeometry* convex_hull(const G& g) {
  return convex_hull_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* boundary(const G& g) {
  return boundary_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* centroid(const G& g) {
  return centroid_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* make_valid(const G& g) {
  return make_valid_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* point_on_surface(const G& g) {
  return point_on_surface_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* densify(const G& g, typename geometry_traits<G>::coordinate_type tolerance) {
  return densify_via_geos(g, static_cast<double>(tolerance));
}

template <ogr_geometry_like G>
OGRGeometry* offset_curve(const G& g, typename geometry_traits<G>::coordinate_type width) {
  return offset_curve_via_geos(g, static_cast<double>(width));
}

template <ogr_geometry_like G>
OGRGeometry* concave_hull(const G& g, double ratio) {
  return concave_hull_via_geos(g, ratio);
}

template <ogr_geometry_like G>
OGRGeometry* minimum_rotated_rectangle(const G& g) {
  return minimum_rotated_rectangle_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* minimum_bounding_circle(const G& g) {
  return minimum_bounding_circle_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* unary_union(const G& g) {
  return unary_union_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* line_merge(const G& g) {
  return line_merge_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* node(const G& g) {
  return node_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* polygonize(const G& g) {
  return polygonize_via_geos(g);
}

template <ogr_geometry_like G>
OGRGeometry* remove_repeated_points(const G& g, double tolerance) {
  return remove_repeated_points_via_geos(g, tolerance);
}

template <ogr_geometry_like G>
OGRGeometry* snap(const G& g, const G& reference, double tolerance) {
  return snap_via_geos(g, reference, tolerance);
}

// Triangulate input sites (MultiPoint / collection). Not tin::delaunay(Vector3*).
template <ogr_geometry_like G>
OGRGeometry* make_delaunay(const G& g, double tolerance = 0.0) {
  return delaunay_via_geos(g, tolerance);
}

template <ogr_geometry_like G>
OGRGeometry* voronoi(const G& g, double tolerance = 0.0) {
  return voronoi_via_geos(g, tolerance);
}

template <ogr_geometry_like G>
OGRGeometry* shortest_line(const G& a, const G& b) {
  return shortest_line_via_geos(a, b);
}

}  // namespace geo

#endif  // GIS_GEO_OPS_SHAPE_H_

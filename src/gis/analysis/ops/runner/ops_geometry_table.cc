// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/ops/runner/op_table.h"

namespace gis {
namespace detail {
namespace {

// File runners use nullptr fn (GeoJSON path via apply_op / IR). Titles feed
// builtin_op_catalog / Processing discoverability (gap-pin P0-F).
constexpr BuiltinOpEntry kGeometryFileOps[] = {
    {"native.buffer", "Buffer", nullptr},
    {"native.clip", "Clip", nullptr},
    {"native.centroid", "Centroid", nullptr},
    {"native.envelope", "Envelope", nullptr},
    {"native.intersection", "Intersection", nullptr},
    {"native.union", "Union", nullptr},
    {"native.difference", "Difference", nullptr},
    {"native.symmetric_difference", "Symmetric difference", nullptr},
    {"native.simplify", "Simplify", nullptr},
    {"native.convex_hull", "Convex hull", nullptr},
    {"native.boundary", "Boundary", nullptr},
    {"native.is_valid", "Is valid", nullptr},
    {"native.is_simple", "Is simple", nullptr},
    {"native.make_valid", "Make valid", nullptr},
    {"native.relate", "Relate (DE-9IM)", nullptr},
    {"native.point_on_surface", "Point on surface", nullptr},
    {"native.densify", "Densify", nullptr},
    {"native.offset_curve", "Offset curve", nullptr},
    {"native.concave_hull", "Concave hull", nullptr},
    {"native.minimum_rotated_rectangle", "Minimum rotated rectangle", nullptr},
    {"native.minimum_bounding_circle", "Minimum bounding circle", nullptr},
    {"native.unary_union", "Unary union", nullptr},
    {"native.line_merge", "Line merge", nullptr},
    {"native.node", "Node", nullptr},
    {"native.polygonize", "Polygonize", nullptr},
    {"native.remove_repeated_points", "Remove repeated points", nullptr},
    {"native.snap", "Snap geometry", nullptr},
    {"native.delaunay", "Delaunay triangulation", nullptr},
    {"native.voronoi", "Voronoi diagram", nullptr},
    {"native.hausdorff_distance", "Hausdorff distance", nullptr},
    {"native.frechet_distance", "Frechet distance", nullptr},
    {"native.shortest_line", "Shortest line", nullptr},
    {"native.covers", "Covers", nullptr},
    {"native.covered_by", "Covered by", nullptr},
    {"native.intersects", "Intersects", nullptr},
    {"native.contains", "Contains", nullptr},
    {"native.within", "Within", nullptr},
    {"native.disjoint", "Disjoint", nullptr},
    {"native.equals", "Equals", nullptr},
    {"native.touches", "Touches", nullptr},
    {"native.crosses", "Crosses", nullptr},
    {"native.overlaps", "Overlaps", nullptr},
    {"native.distance", "Distance", nullptr},
    {"native.area", "Area", nullptr},
    {"native.length", "Length", nullptr},
    {"native.is_empty", "Is empty", nullptr},
    {"native.translate", "Translate geometry (dx, dy)", nullptr},
    {"native.rotate", "Rotate geometry (angle rad, origin)", nullptr},
    {"native.scale", "Scale geometry (scale_x, scale_y, origin)", nullptr},
};

}  // namespace

std::span<const BuiltinOpEntry> geometry_file_op_table() {
  return kGeometryFileOps;
}

}  // namespace detail
}  // namespace gis

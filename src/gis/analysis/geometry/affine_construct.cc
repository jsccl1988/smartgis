// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geometry/affine_construct.h"

#include <cmath>

#include "gis/geo/ops/geometry_traits.h"

#include "ogr_geometry.h"

namespace gis {
namespace detail {
namespace {

bool map_point(OGRPoint* point, const double m[6]) {
  if (!point) {
    return true;
  }
  const double x = point->getX();
  const double y = point->getY();
  point->setX(m[0] * x + m[1] * y + m[2]);
  point->setY(m[3] * x + m[4] * y + m[5]);
  return true;
}

bool map_curve(OGRLineString* line, const double m[6], int dim) {
  if (!line) {
    return true;
  }
  for (int i = 0; i < line->getNumPoints(); ++i) {
    const double x = line->getX(i);
    const double y = line->getY(i);
    const double z = dim >= 3 ? line->getZ(i) : 0.0;
    const double xp = m[0] * x + m[1] * y + m[2];
    const double yp = m[3] * x + m[4] * y + m[5];
    if (dim >= 3) {
      line->setPoint(i, xp, yp, z);
    } else {
      line->setPoint(i, xp, yp);
    }
  }
  return true;
}

bool map_geometry(OGRGeometry* geom, const double m[6]) {
  if (!geom || geom->IsEmpty()) {
    return geom != nullptr;
  }
  const int dim = geo::geometry_traits<OGRGeometry>::coordinate_dimension(*geom);
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPoint) {
    return map_point(static_cast<OGRPoint*>(geom), m);
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    return map_curve(static_cast<OGRLineString*>(geom), m, dim);
  }
  if (flat == wkbPolygon) {
    auto* polygon = static_cast<OGRPolygon*>(geom);
    if (!map_curve(polygon->getExteriorRing(), m, dim)) {
      return false;
    }
    for (int i = 0; i < polygon->getNumInteriorRings(); ++i) {
      if (!map_curve(polygon->getInteriorRing(i), m, dim)) {
        return false;
      }
    }
    return true;
  }
  if (OGRGeometryCollection* collection = geom->toGeometryCollection()) {
    for (int i = 0; i < collection->getNumGeometries(); ++i) {
      if (!map_geometry(collection->getGeometryRef(i), m)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

void multiply_affine(const double a[6], const double b[6], double out[6]) {
  out[0] = a[0] * b[0] + a[1] * b[3];
  out[1] = a[0] * b[1] + a[1] * b[4];
  out[2] = a[0] * b[2] + a[1] * b[5] + a[2];
  out[3] = a[3] * b[0] + a[4] * b[3];
  out[4] = a[3] * b[1] + a[4] * b[4];
  out[5] = a[3] * b[2] + a[4] * b[5] + a[5];
}

}  // namespace

bool apply_affine_2d_inplace(OGRGeometry* geom, const double m[6]) {
  return map_geometry(geom, m);
}

bool affine_translate_2d(OGRGeometry* geom, double dx, double dy) {
  const double m[6] = {1.0, 0.0, dx, 0.0, 1.0, dy};
  return apply_affine_2d_inplace(geom, m);
}

bool affine_rotate_2d(OGRGeometry* geom,
                        double angle_rad,
                        double origin_x,
                        double origin_y) {
  const double c = std::cos(angle_rad);
  const double s = std::sin(angle_rad);
  const double to_origin[6] = {1.0, 0.0, -origin_x, 0.0, 1.0, -origin_y};
  const double rot[6] = {c, -s, 0.0, s, c, 0.0};
  const double from_origin[6] = {1.0, 0.0, origin_x, 0.0, 1.0, origin_y};
  double tmp[6];
  double m[6];
  multiply_affine(rot, to_origin, tmp);
  multiply_affine(from_origin, tmp, m);
  return apply_affine_2d_inplace(geom, m);
}

bool affine_scale_2d(OGRGeometry* geom,
                     double scale_x,
                     double scale_y,
                     double origin_x,
                     double origin_y) {
  const double to_origin[6] = {1.0, 0.0, -origin_x, 0.0, 1.0, -origin_y};
  const double scale[6] = {scale_x, 0.0, 0.0, 0.0, scale_y, 0.0};
  const double from_origin[6] = {1.0, 0.0, origin_x, 0.0, 1.0, origin_y};
  double tmp[6];
  double m[6];
  multiply_affine(scale, to_origin, tmp);
  multiply_affine(from_origin, tmp, m);
  return apply_affine_2d_inplace(geom, m);
}

}  // namespace detail
}  // namespace gis

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/ops/transform.h"

#include "gis/geo/ops/geometry_traits.h"
#include "gis/geo/proj/coordinate_transform.h"

#include "ogr_geometry.h"

#include <string>

namespace geo {
namespace {

bool map_curve(OGRLineString* line, const CoordinateTransform& transform, int dim) {
  if (!line) {
    return true;
  }
  for (int i = 0; i < line->getNumPoints(); ++i) {
    double x = line->getX(i);
    double y = line->getY(i);
    const double z = dim >= 3 ? line->getZ(i) : 0.0;
    if (!transform.transform_xy(x, y)) {
      return false;
    }
    if (dim >= 3) {
      line->setPoint(i, x, y, z);
    } else {
      line->setPoint(i, x, y);
    }
  }
  return true;
}

bool transform_geometry(OGRGeometry* geom, const CoordinateTransform& transform) {
  if (!geom || geom->IsEmpty()) {
    return geom != nullptr;
  }
  const int dim = geometry_traits<OGRGeometry>::coordinate_dimension(*geom);
  const OGRwkbGeometryType flat = wkbFlatten(geom->getGeometryType());
  if (flat == wkbPoint) {
    auto* point = static_cast<OGRPoint*>(geom);
    double x = point->getX();
    double y = point->getY();
    if (!transform.transform_xy(x, y)) {
      return false;
    }
    point->setX(x);
    point->setY(y);
    return true;
  }
  if (flat == wkbLineString || flat == wkbLinearRing) {
    return map_curve(static_cast<OGRLineString*>(geom), transform, dim);
  }
  if (flat == wkbPolygon) {
    auto* polygon = static_cast<OGRPolygon*>(geom);
    if (!map_curve(polygon->getExteriorRing(), transform, dim)) {
      return false;
    }
    for (int i = 0; i < polygon->getNumInteriorRings(); ++i) {
      if (!map_curve(polygon->getInteriorRing(i), transform, dim)) {
        return false;
      }
    }
    return true;
  }
  if (OGRGeometryCollection* collection = geom->toGeometryCollection()) {
    for (int i = 0; i < collection->getNumGeometries(); ++i) {
      if (!transform_geometry(collection->getGeometryRef(i), transform)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

}  // namespace

bool transform_inplace(OGRGeometry* geom, int32_t src_epsg, int32_t dst_epsg) {
  if (!geom) {
    return false;
  }
  const std::string src_crs = "EPSG:" + std::to_string(src_epsg);
  const std::string dst_crs = "EPSG:" + std::to_string(dst_epsg);
  const CoordinateTransform xform(src_crs, dst_crs);
  if (!xform.is_valid()) {
    return false;
  }
  return transform_geometry(geom, xform);
}

}  // namespace geo

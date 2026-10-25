// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/ops/measure.h"

#include "ogr_geometry.h"

namespace geo {

double distance(const OGRGeometry& a, const OGRGeometry& b) {
  return const_cast<OGRGeometry&>(a).Distance(&const_cast<OGRGeometry&>(b));
}

double area(const OGRGeometry& geom) {
  double measured = 0;
  OGRGeometry& mutable_geom = const_cast<OGRGeometry&>(geom);
  if (OGRSurface* surface = mutable_geom.toSurface()) {
    measured = surface->get_Area();
  } else if (OGRGeometryCollection* collection = mutable_geom.toGeometryCollection()) {
    for (int i = 0; i < collection->getNumGeometries(); ++i) {
      if (OGRSurface* part = collection->getGeometryRef(i)->toSurface()) {
        measured += part->get_Area();
      }
    }
  }
  return measured;
}

double length(const OGRGeometry& geom) {
  double measured = 0;
  OGRGeometry& mutable_geom = const_cast<OGRGeometry&>(geom);
  const OGRwkbGeometryType flat = wkbFlatten(mutable_geom.getGeometryType());
  if (flat == wkbLineString || flat == wkbLinearRing) {
    measured = static_cast<OGRLineString&>(mutable_geom).get_Length();
  } else if (OGRGeometryCollection* collection = mutable_geom.toGeometryCollection()) {
    for (int i = 0; i < collection->getNumGeometries(); ++i) {
      OGRGeometry* part = collection->getGeometryRef(i);
      const OGRwkbGeometryType part_flat = wkbFlatten(part->getGeometryType());
      if (part_flat == wkbLineString || part_flat == wkbLinearRing) {
        measured += static_cast<OGRLineString*>(part)->get_Length();
      }
    }
  }
  return measured;
}

bool is_empty(const OGRGeometry& geom) {
  return const_cast<OGRGeometry&>(geom).IsEmpty() != 0;
}

bool is_valid(const OGRGeometry& geom) {
  return const_cast<OGRGeometry&>(geom).IsValid() != 0;
}

bool is_simple(const OGRGeometry& geom) {
  return const_cast<OGRGeometry&>(geom).IsSimple() != 0;
}

}  // namespace geo

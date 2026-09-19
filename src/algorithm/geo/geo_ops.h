// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_GEO_GEO_OPS_H_
#define ALGORITHM_GEO_GEO_OPS_H_

#include "algorithm/geo/geometry_traits.h"

#if defined(GEO_EXPORTS)
#define GEO_OPS_API __declspec(dllexport)
#else
#define GEO_OPS_API __declspec(dllimport)
#endif

namespace geo {

// OGR Buffer if GDAL was built with GEOS; otherwise geos_c (same install).
// Caller owns the result.
GEO_OPS_API OGRGeometry* buffer_via_geos_or_ogr(const OGRGeometry& geom,
                                                double width);

// Buffer via the shipped GEOS stack. Z is not sent through GEOS; the
// instance dimension is restored.
template <geometry_like G>
OGRGeometry* buffer(const G& g,
                    typename geometry_traits<G>::coordinate_type width) {
  OGRGeometry* out = buffer_via_geos_or_ogr(g, static_cast<double>(width));
  if (out != nullptr) {
    out->setCoordinateDimension(geometry_traits<G>::coordinate_dimension(g));
  }
  return out;
}

}  // namespace geo

#endif  // ALGORITHM_GEO_GEO_OPS_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_GEO_GEO_OPS_H_
#define ALGORITHM_GEO_GEO_OPS_H_

#include "algorithm/geo/geometry_traits.h"

namespace geo {

// Buffer via OGR (GEOS inside the shipped GDAL stack). Caller owns the
// result. Z is not sent through GEOS; the instance dimension is restored.
template <geometry_like G>
OGRGeometry* buffer(const G& g,
                    typename geometry_traits<G>::coordinate_type width) {
  OGRGeometry* out = g.Buffer(static_cast<double>(width));
  if (out != nullptr) {
    out->setCoordinateDimension(geometry_traits<G>::coordinate_dimension(g));
  }
  return out;
}

}  // namespace geo

#endif  // ALGORITHM_GEO_GEO_OPS_H_

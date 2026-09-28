// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_GEO_GEOMETRY_TRAITS_H_
#define ALGORITHM_GEO_GEOMETRY_TRAITS_H_

#include "ogr_geometry.h"

#include <type_traits>

namespace geo {

// Instance OGC coordinateDimension (2 or 3) for product OGR types.
// Compile-time dimension lives on vectors, not on Geometry.
template <typename G>
struct geometry_traits;

template <typename G>
  requires std::is_base_of_v<OGRGeometry, G>
struct geometry_traits<G> {
  using coordinate_type = double;

  static int coordinate_dimension(const G& g) {
    return g.getCoordinateDimension();
  }
};

template <typename G>
concept geometry_like = requires(const G& g) {
  typename geometry_traits<G>::coordinate_type;
  geometry_traits<G>::coordinate_dimension(g);
};

}  // namespace geo

#endif  // ALGORITHM_GEO_GEOMETRY_TRAITS_H_

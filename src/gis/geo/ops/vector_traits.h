// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_VECTOR_TRAITS_H_
#define GIS_GEO_OPS_VECTOR_TRAITS_H_

#include "base/math/math.h"

namespace geo {

// Compile-time dimension for scene vectors (Vector2 / Vector3 / Vector4).
// The type already exposes `dimension` and `coordinate_type`.
template <typename V>
struct vector_traits {
  using coordinate_type = typename V::coordinate_type;
  static constexpr int dimension = V::dimension;
};

}  // namespace geo

#endif  // GIS_GEO_OPS_VECTOR_TRAITS_H_

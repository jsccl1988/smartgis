// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_BASE_MATH_TRAITS_VECTOR_TRAITS_H_
#define SMT_BASE_MATH_TRAITS_VECTOR_TRAITS_H_

#include "base/math/linear/point.h"
#include "base/math/linear/vector.h"

namespace base {

// Compile-time dimension for scene vectors / Point2. Types already expose
// `dimension` and `coordinate_type`; this trait is the dispatch seam.
template <typename V>
struct vector_traits {
  using coordinate_type = typename V::coordinate_type;
  static constexpr int dimension = V::dimension;
};

template <typename V>
concept vector_like = requires {
  typename vector_traits<V>::coordinate_type;
} && (vector_traits<V>::dimension >= 2);

}  // namespace base

#endif  // SMT_BASE_MATH_TRAITS_VECTOR_TRAITS_H_

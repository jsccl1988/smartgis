// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_OPS_VECTOR_TRAITS_H_
#define GIS_GEO_OPS_VECTOR_TRAITS_H_

#include "base/math/traits/vector_traits.h"

namespace geo {

// Scene-vector traits live in `base`. GIS re-exports the same template so
// algorithm TUs keep writing `geo::vector_traits<V>`.
template <typename V>
using vector_traits = ::base::vector_traits<V>;

}  // namespace geo

#endif  // GIS_GEO_OPS_VECTOR_TRAITS_H_

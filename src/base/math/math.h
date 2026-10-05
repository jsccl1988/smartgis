// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_MATH_H_
#define BASE_MATH_MATH_H_

// Aggregator for product scene math. Types live in `namespace base`.
// Physical layers: scalar → linear → traits → geom → xform → simd.

#include "base/math/scalar/constants.h"
#include "base/math/linear/affine2.h"
#include "base/math/linear/point.h"
#include "base/math/linear/vector.h"
#include "base/math/linear/matrix.h"
#include "base/math/linear/quat.h"
#include "base/math/traits/vector_traits.h"
#include "base/math/geom/cull.h"
#include "base/math/geom/aabb.h"
#include "base/math/geom/obb.h"
#include "base/math/geom/plane.h"
#include "base/math/geom/ray.h"
#include "base/math/geom/frustum.h"
#include "base/math/xform/transform.h"
#include "base/math/xform/interpolate.h"
#include "base/math/simd/simd.h"

#endif  // BASE_MATH_MATH_H_

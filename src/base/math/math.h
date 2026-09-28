// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_MATH_H_
#define SMT_RENDER_MATH_MATH_H_

#include "base/math/constants.h"
#include "base/math/vector.h"
#include "base/math/matrix.h"
#include "base/math/quat.h"
#include "base/math/aabb.h"
#include "base/math/obb.h"
#include "base/math/plane.h"
#include "base/math/ray.h"
#include "base/math/frustum.h"
#include "base/math/transform.h"
#include "base/math/interpolate.h"
#include "base/math/simd.h"

namespace geo {

// Convenience alias historically provided by mathlib.h.
using Vector = render::Vector2;

}  // namespace geo

#endif  // SMT_RENDER_MATH_MATH_H_

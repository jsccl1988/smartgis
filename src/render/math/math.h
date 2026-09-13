// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_MATH_H_
#define SMT_RENDER_MATH_MATH_H_

#include "render/math/constants.h"
#include "render/math/vector.h"
#include "render/math/matrix.h"
#include "render/math/quat.h"
#include "render/math/aabb.h"
#include "render/math/obb.h"
#include "render/math/plane.h"
#include "render/math/ray.h"
#include "render/math/frustum.h"
#include "render/math/transform.h"
#include "render/math/interpolate.h"
#include "render/math/simd.h"

namespace geo {

// Convenience alias historically provided by mathlib.h.
using Vector = render::Vector2;

}  // namespace geo

#endif  // SMT_RENDER_MATH_MATH_H_

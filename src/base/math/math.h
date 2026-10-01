// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_MATH_H_
#define SMT_RENDER_MATH_MATH_H_

#include "base/math/constants.h"
#include "base/math/affine2.h"
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
using Vector = base::Vector2;

}  // namespace geo

// Temporary: historical `render::` math names. Remove after call-site cutover.
// Prefer `using T = ::base::T` for types (MSVC dllimport mangling); keep
// using-declarations for free functions / constants.
namespace render {
using Vector2 = ::base::Vector2;
using Vector3 = ::base::Vector3;
using Vector4 = ::base::Vector4;
using Matrix = ::base::Matrix;
using Quat = ::base::Quat;
using Aabb = ::base::Aabb;
using Obb = ::base::Obb;
using Plane = ::base::Plane;
using Ray = ::base::Ray;
using Frustum = ::base::Frustum;
using Transform = ::base::Transform;
using TransformStack = ::base::TransformStack;
using LpToDp2 = ::base::LpToDp2;
using PlaneSide = ::base::PlaneSide;
using CullResult = ::base::CullResult;
using FrustumHit = ::base::FrustumHit;
using ::base::dot;
using ::base::cross;
using ::base::triangle_normal;
using ::base::lerp;
using ::base::nlerp;
using ::base::slerp;
using ::base::transform_xy;
using ::base::inverse_xy;
using ::base::transform_xy_batch;
using ::base::normalize_batch;
using ::base::transform_points_batch;
using ::base::deg_to_rad;
using ::base::rad_to_deg;
using ::base::kPi;
using ::base::kEpsilon;
}  // namespace render

#endif  // SMT_RENDER_MATH_MATH_H_

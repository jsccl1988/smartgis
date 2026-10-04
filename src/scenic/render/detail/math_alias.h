// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_MATH_ALIAS_H_
#define SCENIC_DETAIL_MATH_ALIAS_H_

#include "base/math/math.h"

// Copy TUs historically shared leftover `namespace render` with product
// math aliases in base/math. Public identity is `scenic`; pull the same
// types from `base::` (already a scenic_impl dep). Not leftover GIS.
namespace scenic {
namespace detail {
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
}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_DETAIL_MATH_ALIAS_H_

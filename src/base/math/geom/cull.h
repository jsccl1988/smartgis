// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_GEOM_CULL_H_
#define BASE_MATH_GEOM_CULL_H_

namespace base {

// Classification of a point relative to a plane.
enum class PlaneSide {
  kFront = 0,
  kBack = 1,
  kPlanar = 2,
};

// Result of frustum / multi-plane cull against a volume.
enum class CullResult {
  kClipped = 3,
  kCulled = 4,
  kVisible = 5,
};

// Frustum vs volume classification.
enum class FrustumHit {
  kOutside = 0,
  kIntersect = 1,
  kInside = 2,
};

}  // namespace base

namespace render {
using ::base::PlaneSide;
using ::base::CullResult;
using ::base::FrustumHit;
}  // namespace render

#endif  // BASE_MATH_GEOM_CULL_H_

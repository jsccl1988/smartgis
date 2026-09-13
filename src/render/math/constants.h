// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_CONSTANTS_H_
#define SMT_RENDER_MATH_CONSTANTS_H_

namespace render {

inline constexpr double kPi = 3.14159265;
inline constexpr double kHalfPi = 1.5707963;
inline constexpr double kTwoPi = 6.2831853;
inline constexpr float kEpsilon = 0.00001f;
inline constexpr float kGravity = -32.174f;

constexpr float deg_to_rad(float a) {
  return static_cast<float>(kPi / 180.0 * static_cast<double>(a));
}

constexpr float rad_to_deg(float a) {
  return static_cast<float>(180.0 / kPi * static_cast<double>(a));
}

// Leftover call sites (e.g. northarray) may still use these macros.
#ifndef DEG2RAD
#define DEG2RAD(a) (::render::deg_to_rad(static_cast<float>(a)))
#endif
#ifndef RAD2DEG
#define RAD2DEG(a) (::render::rad_to_deg(static_cast<float>(a)))
#endif
#ifndef PI
#define PI (::render::kPi)
#endif

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

}  // namespace render

#endif  // SMT_RENDER_MATH_CONSTANTS_H_

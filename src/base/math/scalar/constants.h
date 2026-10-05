// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_SCALAR_CONSTANTS_H_
#define BASE_MATH_SCALAR_CONSTANTS_H_

namespace base {

inline constexpr double kPi = 3.14159265;
inline constexpr double kHalfPi = 1.5707963;
inline constexpr double kTwoPi = 6.2831853;
inline constexpr float kEpsilon = 0.00001f;
inline constexpr float kGravity = -32.174f;
inline constexpr double kInvalidCoord = 1e10;

constexpr float deg_to_rad(float a) {
  return static_cast<float>(kPi / 180.0 * static_cast<double>(a));
}

constexpr float rad_to_deg(float a) {
  return static_cast<float>(180.0 / kPi * static_cast<double>(a));
}

// Leftover call sites (e.g. northarray) may still use these macros.
#ifndef DEG2RAD
#define DEG2RAD(a) (::base::deg_to_rad(static_cast<float>(a)))
#endif
#ifndef RAD2DEG
#define RAD2DEG(a) (::base::rad_to_deg(static_cast<float>(a)))
#endif
#ifndef PI
#define PI (::base::kPi)
#endif

}  // namespace base

namespace render {
using ::base::kPi;
using ::base::kHalfPi;
using ::base::kTwoPi;
using ::base::kEpsilon;
using ::base::kGravity;
using ::base::kInvalidCoord;
using ::base::deg_to_rad;
using ::base::rad_to_deg;
}  // namespace render

#endif  // BASE_MATH_SCALAR_CONSTANTS_H_

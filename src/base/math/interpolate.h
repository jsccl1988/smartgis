// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_INTERPOLATE_H_
#define SMT_RENDER_MATH_INTERPOLATE_H_

#include "base/math/quat.h"
#include "base/math/vector.h"

#include <cmath>

namespace base {

template <typename T>
T lerp(const T& a, const T& b, float t) {
  return a * (1.0f - t) + b * t;
}

inline float lerp(float a, float b, float t) {
  return a * (1.0f - t) + b * t;
}

// Normalized lerp of quaternions (fast; not constant angular speed).
inline Quat nlerp(const Quat& a, const Quat& b, float t) {
  Eigen::Quaternionf qa = a.eigen();
  Eigen::Quaternionf qb = b.eigen();
  if (qa.dot(qb) < 0.0f) {
    qb.coeffs() = -qb.coeffs();
  }
  Eigen::Quaternionf out;
  out.coeffs() = qa.coeffs() * (1.0f - t) + qb.coeffs() * t;
  out.normalize();
  Quat r;
  r.from_eigen(out);
  return r;
}

// Spherical linear interpolation of quaternions.
inline Quat slerp(const Quat& a, const Quat& b, float t) {
  Eigen::Quaternionf qa = a.eigen();
  Eigen::Quaternionf qb = b.eigen();
  Quat out;
  out.from_eigen(qa.slerp(t, qb));
  return out;
}

}  // namespace base

namespace render {
using ::base::lerp;
using ::base::nlerp;
using ::base::slerp;
}  // namespace render

#endif  // SMT_RENDER_MATH_INTERPOLATE_H_

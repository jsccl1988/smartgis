// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_INTERPOLATE_H_
#define SMT_RENDER_MATH_INTERPOLATE_H_

#include "render/math/quat.h"
#include "render/math/vector.h"

#include <cmath>

namespace render {

template <typename T>
T lerp(const T& a, const T& b, float t) {
  return a * (1.0f - t) + b * t;
}

inline float lerp(float a, float b, float t) {
  return a * (1.0f - t) + b * t;
}

// Normalized lerp of quaternions (fast; not constant angular speed).
inline Quat nlerp(const Quat& a, const Quat& b, float t) {
  Quat b_adj = b;
  if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f) {
    b_adj = Quat(-b.x, -b.y, -b.z, -b.w);
  }
  Quat out = Quat(lerp(a.x, b_adj.x, t), lerp(a.y, b_adj.y, t),
                  lerp(a.z, b_adj.z, t), lerp(a.w, b_adj.w, t));
  out.normalize();
  return out;
}

// Spherical linear interpolation of quaternions.
inline Quat slerp(const Quat& a, const Quat& b, float t) {
  float cos_omega = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  Quat b_adj = b;
  if (cos_omega < 0.0f) {
    cos_omega = -cos_omega;
    b_adj = Quat(-b.x, -b.y, -b.z, -b.w);
  }
  if (cos_omega > 0.9995f) {
    return nlerp(a, b_adj, t);
  }
  const float omega = std::acos(cos_omega);
  const float sin_omega = std::sin(omega);
  const float w0 = std::sin((1.0f - t) * omega) / sin_omega;
  const float w1 = std::sin(t * omega) / sin_omega;
  return Quat(a.x * w0 + b_adj.x * w1, a.y * w0 + b_adj.y * w1,
              a.z * w0 + b_adj.z * w1, a.w * w0 + b_adj.w * w1);
}

}  // namespace render

#endif  // SMT_RENDER_MATH_INTERPOLATE_H_

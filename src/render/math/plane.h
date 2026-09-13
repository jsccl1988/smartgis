// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_PLANE_H_
#define SMT_RENDER_MATH_PLANE_H_

#include "render/math/constants.h"
#include "render/math/vector.h"

namespace render {

class Aabb;
class Obb;
class Ray;

// Plane in Hessian form: n·x + d = 0 (leftover field names retained).
class Plane {
 public:
  Vector4 m_vcN;
  Vector4 m_vcPoint;
  float m_fD;

  Plane() {}

  void set(const Vector4& n, const Vector4& p) {
    m_fD = -dot(n, p);
    m_vcN = n;
    m_vcPoint = p;
  }
  void set(const Vector4& n, const Vector4& p, float d) {
    m_vcN = n;
    m_fD = d;
    m_vcPoint = p;
  }
  void set(const Vector4& v0, const Vector4& v1, const Vector4& v2) {
    const Vector4 edge1 = v1 - v0;
    const Vector4 edge2 = v2 - v0;
    m_vcN = edge1.cross(edge2);
    m_vcN.normalize();
    m_fD = -dot(m_vcN, v0);
    m_vcPoint = v0;
  }

  float distance(const Vector4& point);
  PlaneSide classify(const Vector4& point);

  bool clip(const Ray* ray, float length, Ray* front_out, Ray* back_out);
  bool intersects(const Vector4& v0, const Vector4& v1, const Vector4& v2);
  bool intersects(const Plane& plane, Ray* intersection);
  bool intersects(const Aabb& aabb);
  bool intersects(const Obb& obb);
};

}  // namespace render

#endif  // SMT_RENDER_MATH_PLANE_H_

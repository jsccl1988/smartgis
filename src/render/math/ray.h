// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_RAY_H_
#define SMT_RENDER_MATH_RAY_H_

#include "render/math/vector.h"

namespace render {

class Aabb;
class Matrix;
class Obb;
class Plane;

// Ray with origin + direction (leftover field names retained).
class Ray {
 public:
  Vector4 m_vcOrig;
  Vector4 m_vcDir;

  Ray() {}

  void set(Vector4 orig, Vector4 dir) {
    m_vcOrig = orig;
    m_vcDir = dir;
  }
  void de_transform(const Matrix& m);

  bool intersects(const Vector4& v0, const Vector4& v1, const Vector4& v2,
                  bool cull, float* t) const;
  bool intersects(const Vector4& v0, const Vector4& v1, const Vector4& v2,
                  bool cull, float length, float* t) const;
  bool intersects(const Plane& plane, bool cull, float* t, Vector4* hit) const;
  bool intersects(const Plane& plane, bool cull, float length, float* t,
                  Vector4* hit) const;
  bool intersects(const Aabb& aabb, float* t) const;
  bool intersects(const Aabb& aabb, float length, float* t) const;
  bool intersects(const Obb& obb, float* t) const;
  bool intersects(const Obb& obb, float length, float* t) const;
};

}  // namespace render

#endif  // SMT_RENDER_MATH_RAY_H_

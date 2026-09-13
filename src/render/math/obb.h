// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_OBB_H_
#define SMT_RENDER_MATH_OBB_H_

#include "render/math/constants.h"
#include "render/math/vector.h"

namespace render {

class Matrix;
class Plane;

// Oriented bounding box (center + orthonormal axes + half-extents).
class Obb {
 public:
  float fA0, fA1, fA2;
  Vector4 vcA0, vcA1, vcA2;
  Vector4 vcCenter;

  Obb() {}

  void de_transform(const Obb& obb, const Matrix& m);
  bool intersects(const Obb& obb);
  bool intersects(const Vector4& v0, const Vector4& v1, const Vector4& v2);
  CullResult cull(const Plane* planes, int num_planes);

 private:
  void obb_proj(const Obb& obb, const Vector4& v, float* min_out,
                float* max_out);
  void tri_proj(const Vector4& v0, const Vector4& v1, const Vector4& v2,
                const Vector4& v, float* min_out, float* max_out);
};

}  // namespace render

#endif  // SMT_RENDER_MATH_OBB_H_

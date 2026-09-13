// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_FRUSTUM_H_
#define SMT_RENDER_MATH_FRUSTUM_H_

#include "render/math/constants.h"
#include "render/math/matrix.h"
#include "render/math/plane.h"

namespace render {

class Aabb;
class Obb;

// View frustum as six clip planes; built from a view-projection matrix.
class Frustum {
 public:
  Plane planes[6];

  static Frustum from_view_proj(const Matrix& view_proj);
  static Frustum from_planes(const Plane* src, int count);

  FrustumHit classify(const Aabb& aabb) const;
  FrustumHit classify(const Obb& obb) const;
  bool intersects(const Aabb& aabb) const;
  bool intersects(const Obb& obb) const;
};

}  // namespace render

#endif  // SMT_RENDER_MATH_FRUSTUM_H_

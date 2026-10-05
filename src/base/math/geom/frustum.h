// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_GEOM_FRUSTUM_H_
#define BASE_MATH_GEOM_FRUSTUM_H_

#include "base/math/geom/cull.h"
#include "base/math/geom/plane.h"
#include "base/math/linear/matrix.h"

#include <cmath>

namespace base {

class Aabb;
class Obb;

inline void frustum_normalize_plane(Plane* p) {
  const float len = std::sqrt(p->m_vcN.x * p->m_vcN.x + p->m_vcN.y * p->m_vcN.y +
                              p->m_vcN.z * p->m_vcN.z);
  if (len > 1e-8f) {
    p->m_vcN.x /= len;
    p->m_vcN.y /= len;
    p->m_vcN.z /= len;
    p->m_fD /= len;
  }
  p->m_vcPoint = p->m_vcN * (-p->m_fD);
}

// View frustum as six clip planes; built from a view-projection matrix.
// Plane normals point **outward** (compatible with Aabb::cull).
class Frustum {
 public:
  Plane planes[6];

  // Column-major 4x4 clip product (GL glGetFloatv layout / NeHe extract).
  static Frustum from_column_major_clip(const float clip[16]) {
    Frustum f;
    // NeHe / Gribb-Hartmann inward planes, then negate for Aabb::cull.
    const float inward[6][4] = {
        {clip[3] - clip[0], clip[7] - clip[4], clip[11] - clip[8],
         clip[15] - clip[12]},
        {clip[3] + clip[0], clip[7] + clip[4], clip[11] + clip[8],
         clip[15] + clip[12]},
        {clip[3] + clip[1], clip[7] + clip[5], clip[11] + clip[9],
         clip[15] + clip[13]},
        {clip[3] - clip[1], clip[7] - clip[5], clip[11] - clip[9],
         clip[15] - clip[13]},
        {clip[3] - clip[2], clip[7] - clip[6], clip[11] - clip[10],
         clip[15] - clip[14]},
        {clip[3] + clip[2], clip[7] + clip[6], clip[11] + clip[10],
         clip[15] + clip[14]},
    };
    for (int i = 0; i < 6; ++i) {
      f.planes[i].m_vcN.set(inward[i][0], inward[i][1], inward[i][2]);
      f.planes[i].m_fD = inward[i][3];
      frustum_normalize_plane(&f.planes[i]);
      f.planes[i].m_vcN = f.planes[i].m_vcN * -1.f;
      f.planes[i].m_fD = -f.planes[i].m_fD;
      f.planes[i].m_vcPoint = f.planes[i].m_vcN * (-f.planes[i].m_fD);
    }
    return f;
  }

  // Row-major leftover MVP → column-major pack → extract.
  static Frustum from_view_proj(const Matrix& vp) {
    const float clip[16] = {vp._11, vp._21, vp._31, vp._41, vp._12, vp._22,
                            vp._32, vp._42, vp._13, vp._23, vp._33, vp._43,
                            vp._14, vp._24, vp._34, vp._44};
    return from_column_major_clip(clip);
  }

  static Frustum from_planes(const Plane* src, int count) {
    Frustum f;
    const int n = count < 6 ? count : 6;
    for (int i = 0; i < n; ++i) {
      f.planes[i] = src[i];
    }
    return f;
  }

  FrustumHit classify(const Aabb& aabb) const;
  FrustumHit classify(const Obb& obb) const;
  bool intersects(const Aabb& aabb) const;
  bool intersects(const Obb& obb) const;
};

}  // namespace base

namespace render {
using ::base::Frustum;
}  // namespace render

#endif  // BASE_MATH_GEOM_FRUSTUM_H_

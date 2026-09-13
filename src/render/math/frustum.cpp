// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/frustum.h"

#include "render/math/aabb.h"
#include "render/math/obb.h"

#include <cmath>

namespace render {
namespace {

void normalize_plane(Plane* p) {
  const float len = p->m_vcN.length();
  if (len > kEpsilon) {
    p->m_vcN *= (1.0f / len);
    p->m_fD /= len;
  }
}

}  // namespace

Frustum Frustum::from_view_proj(const Matrix& vp) {
  Frustum f;
  // Extract clip planes from column-combined VP (row-major storage).
  // Right
  f.planes[0].m_vcN.set(vp._14 - vp._11, vp._24 - vp._21, vp._34 - vp._31);
  f.planes[0].m_fD = vp._44 - vp._41;
  // Left
  f.planes[1].m_vcN.set(vp._14 + vp._11, vp._24 + vp._21, vp._34 + vp._31);
  f.planes[1].m_fD = vp._44 + vp._41;
  // Bottom
  f.planes[2].m_vcN.set(vp._14 + vp._12, vp._24 + vp._22, vp._34 + vp._32);
  f.planes[2].m_fD = vp._44 + vp._42;
  // Top
  f.planes[3].m_vcN.set(vp._14 - vp._12, vp._24 - vp._22, vp._34 - vp._32);
  f.planes[3].m_fD = vp._44 - vp._42;
  // Far
  f.planes[4].m_vcN.set(vp._14 - vp._13, vp._24 - vp._23, vp._34 - vp._33);
  f.planes[4].m_fD = vp._44 - vp._43;
  // Near
  f.planes[5].m_vcN.set(vp._14 + vp._13, vp._24 + vp._23, vp._34 + vp._33);
  f.planes[5].m_fD = vp._44 + vp._43;
  for (int i = 0; i < 6; ++i) {
    normalize_plane(&f.planes[i]);
    f.planes[i].m_vcPoint = f.planes[i].m_vcN * (-f.planes[i].m_fD);
  }
  return f;
}

Frustum Frustum::from_planes(const Plane* src, int count) {
  Frustum f;
  const int n = count < 6 ? count : 6;
  for (int i = 0; i < n; ++i) {
    f.planes[i] = src[i];
  }
  return f;
}

FrustumHit Frustum::classify(const Aabb& aabb) const {
  const CullResult r = const_cast<Aabb&>(aabb).cull(planes, 6);
  if (r == CullResult::kCulled) {
    return FrustumHit::kOutside;
  }
  if (r == CullResult::kClipped) {
    return FrustumHit::kIntersect;
  }
  return FrustumHit::kInside;
}

FrustumHit Frustum::classify(const Obb& obb) const {
  const CullResult r = const_cast<Obb&>(obb).cull(planes, 6);
  if (r == CullResult::kCulled) {
    return FrustumHit::kOutside;
  }
  if (r == CullResult::kClipped) {
    return FrustumHit::kIntersect;
  }
  return FrustumHit::kInside;
}

bool Frustum::intersects(const Aabb& aabb) const {
  return classify(aabb) != FrustumHit::kOutside;
}

bool Frustum::intersects(const Obb& obb) const {
  return classify(obb) != FrustumHit::kOutside;
}

}  // namespace render

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/math/geom/frustum.h"

#include "base/math/geom/aabb.h"
#include "base/math/geom/obb.h"

namespace base {

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

}  // namespace base

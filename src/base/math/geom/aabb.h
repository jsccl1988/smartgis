// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_MATH_GEOM_AABB_H_
#define BASE_MATH_GEOM_AABB_H_

#include "base/math/detail/eigen.h"
#include "base/math/geom/cull.h"
#include "base/math/linear/vector.h"
#include "base/math/scalar/constants.h"

namespace base {

class Obb;
class Plane;

// Axis-aligned bounding box in leftover field layout (vcMin/vcMax/vcCenter).
class Aabb {
 public:
  Vector4 vcMin;
  Vector4 vcMax;
  Vector4 vcCenter;

  Aabb();
  Aabb(Vector4 min_pt, Vector4 max_pt);

  using Box = Eigen::AlignedBox3f;
  Box eigen() const {
    return Box(vcMin.xyz(), vcMax.xyz());
  }
  void from_eigen(const Box& box) {
    // Parenthesize to defeat Win32 min/max macros when windows.h is included.
    const detail::EigenVec3 mn = (box.min)();
    const detail::EigenVec3 mx = (box.max)();
    vcMin.set(mn.x(), mn.y(), mn.z());
    vcMax.set(mx.x(), mx.y(), mx.z());
    vcCenter = (vcMax + vcMin) * 0.5f;
  }

  bool is_init() const;
  void merge(const Aabb& aabb);
  void merge(double x, double y, double z);
  void merge(const Vector4& v);
  void intersect(Aabb const& other);
  bool intersects(Aabb const& other) const;
  bool contains(Aabb const& other) const;
  bool contains(Vector3 const& other) const;

  void construct(const Obb* obb);
  CullResult cull(const Plane* planes, int num_planes);
  void get_planes(Plane* planes);
  bool intersects(const Vector4& vc0);
};

}  // namespace base

namespace render {
using Aabb = ::base::Aabb;
}  // namespace render

#endif  // BASE_MATH_GEOM_AABB_H_

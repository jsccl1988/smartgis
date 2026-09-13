// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_RENDER_MATH_AABB_H_
#define SMT_RENDER_MATH_AABB_H_

#include "render/math/constants.h"
#include "render/math/vector.h"

namespace render {

class Obb;
class Plane;
class Ray;

// Axis-aligned bounding box in leftover field layout (vcMin/vcMax/vcCenter).
class Aabb {
 public:
  Vector4 vcMin;
  Vector4 vcMax;
  Vector4 vcCenter;

  Aabb();
  Aabb(Vector4 min_pt, Vector4 max_pt);

  bool is_init() const;
  void merge(const Aabb& aabb);
  void merge(double x, double y, double z);
  void merge(const Vector4& v);
  void intersect(Aabb const& other);
  bool intersects(Aabb const& other) const;
  bool contains(Aabb const& other) const;
  bool contains(Vector3 const& other) const;
  bool contains(const Ray& ray, float length);

  void construct(const Obb* obb);
  CullResult cull(const Plane* planes, int num_planes);
  void get_planes(Plane* planes);
  bool intersects(const Vector4& vc0);
};

}  // namespace render

#endif  // SMT_RENDER_MATH_AABB_H_

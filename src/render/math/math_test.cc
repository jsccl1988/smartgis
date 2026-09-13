// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/math/math.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
  using render::Vector3;
  using render::Vector4;
  using render::Aabb;
  using render::Ray;
  using render::Quat;
  using render::lerp;
  using render::nlerp;
  using render::TransformStack;
  using render::Frustum;
  using render::Matrix;
  using render::dot;

  Vector3 a(1, 0, 0);
  Vector3 b(0, 1, 0);
  assert(std::fabs(dot(a, b)) < 1e-6f);
  assert(std::fabs(a.cross(b).z - 1.f) < 1e-6f);

  Aabb box(Vector4(-1, -1, -1), Vector4(1, 1, 1));
  Ray ray;
  ray.set(Vector4(0, 0, -5), Vector4(0, 0, 1));
  float t = 0;
  assert(ray.intersects(box, &t));

  assert(std::fabs(lerp(0.f, 10.f, 0.5f) - 5.f) < 1e-6f);
  Quat q0;
  Quat q1;
  q1.from_euler(0, 3.14159265f / 2.f, 0);
  Quat qn = nlerp(q0, q1, 0.f);
  assert(std::fabs(qn.w - 1.f) < 1e-5f);

  TransformStack stack;
  stack.translate(1, 2, 3);
  assert(std::fabs(stack.matrix()._41 - 1.f) < 1e-6f);

  Matrix vp;
  vp.identity();
  Frustum fr = Frustum::from_view_proj(vp);
  assert(fr.intersects(box));

  std::printf("math_test ok\n");
  return 0;
}

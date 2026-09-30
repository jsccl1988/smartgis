// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/math/math.h"

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

  {
    using render::LpToDp2;
    using render::transform_xy;
    using render::transform_xy_batch;
    LpToDp2 a;
    a.wox = 10.f;
    a.woy = 20.f;
    a.vox = 5.f;
    a.voy = 7.f;
    a.scale = 2.f;
    a.view_h = 100.f;
    a.flip_y = true;
    long x = 0;
    long y = 0;
    transform_xy(a, 12.f, 24.f, &x, &y);
    // X = LONG(5 + (12-10)*2 + 0.5) = 9
    // Y = LONG(7 + (24-20)*2 + 0.5) = 15; flip → LONG(100-15)=85
    assert(x == 9);
    assert(y == 85);

    const float xy_in[] = {12.f, 24.f, 10.f, 20.f};
    long xy_out[4] = {};
    transform_xy_batch(a, xy_in, xy_out);
    assert(xy_out[0] == 9 && xy_out[1] == 85);
    // origin maps to (vox+0.5, view_h-(voy+0.5))
    assert(xy_out[2] == 5);
    assert(xy_out[3] == static_cast<long>(100.f - 7));
  }

  std::printf("math_test ok\n");
  return 0;
}

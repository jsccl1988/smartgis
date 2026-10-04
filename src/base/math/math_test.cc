// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/math/math.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace {

// Leftover inward-plane IsBoxIn (NeHe), for golden comparison only.
bool leftover_is_box_in(const float planes[6][4], float max_x, float max_y,
                        float max_z, float min_x, float min_y, float min_z) {
  for (int i = 0; i < 6; i++) {
    if (planes[i][0] * min_x + planes[i][1] * min_y + planes[i][2] * min_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * max_x + planes[i][1] * min_y + planes[i][2] * min_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * min_x + planes[i][1] * max_y + planes[i][2] * min_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * max_x + planes[i][1] * max_y + planes[i][2] * min_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * min_x + planes[i][1] * min_y + planes[i][2] * max_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * max_x + planes[i][1] * min_y + planes[i][2] * max_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * min_x + planes[i][1] * max_y + planes[i][2] * max_z +
            planes[i][3] >
        0)
      continue;
    if (planes[i][0] * max_x + planes[i][1] * max_y + planes[i][2] * max_z +
            planes[i][3] >
        0)
      continue;
    return false;
  }
  return true;
}

void leftover_extract_inward(const base::Matrix& mvp, float planes[6][4]) {
  const float clip[16] = {mvp._11, mvp._21, mvp._31, mvp._41, mvp._12, mvp._22,
                          mvp._32, mvp._42, mvp._13, mvp._23, mvp._33, mvp._43,
                          mvp._14, mvp._24, mvp._34, mvp._44};
  const float raw[6][4] = {
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
    const float len =
        std::sqrt(raw[i][0] * raw[i][0] + raw[i][1] * raw[i][1] +
                  raw[i][2] * raw[i][2]);
    if (len > 1e-8f) {
      planes[i][0] = raw[i][0] / len;
      planes[i][1] = raw[i][1] / len;
      planes[i][2] = raw[i][2] / len;
      planes[i][3] = raw[i][3] / len;
    } else {
      planes[i][0] = raw[i][0];
      planes[i][1] = raw[i][1];
      planes[i][2] = raw[i][2];
      planes[i][3] = raw[i][3];
    }
  }
}

}  // namespace

int main() {
  using base::Vector3;
  using base::Vector4;
  using base::Aabb;
  using base::Ray;
  using base::Quat;
  using base::lerp;
  using base::nlerp;
  using base::slerp;
  using base::TransformStack;
  using base::Frustum;
  using base::Matrix;
  using base::dot;

  static_assert(base::vector_traits<base::Vector2>::dimension == 2);
  static_assert(base::vector_traits<base::Vector3>::dimension == 3);
  static_assert(base::vector_traits<base::Vector4>::dimension == 4);
  static_assert(base::vector_traits<base::Point2<float>>::dimension == 2);
  static_assert(base::vector_like<base::Vector3>);

  Vector3 a(1, 0, 0);
  Vector3 b(0, 1, 0);
  assert(std::fabs(dot(a, b)) < 1e-6f);
  assert(std::fabs(a.cross(b).z - 1.f) < 1e-6f);

  Aabb box(Vector4(-1, -1, -1), Vector4(1, 1, 1));
  Ray ray;
  ray.set(Vector4(0, 0, -5), Vector4(0, 0, 1));
  float t = 0;
  assert(ray.intersects(box, &t));
  assert(box.intersects(box));
  assert(box.contains(Vector3(0, 0, 0)));

  Matrix rx;
  rx.rotate_x(base::deg_to_rad(90.f));
  Vector4 vy = rx.transform_vector(Vector4(0, 1, 0, 0));
  assert(std::fabs(vy.z - 1.f) < 1e-5f || std::fabs(vy.z + 1.f) < 1e-5f);

  assert(std::fabs(lerp(0.f, 10.f, 0.5f) - 5.f) < 1e-6f);
  Quat q0;
  Quat q1;
  q1.from_euler(0, 3.14159265f / 2.f, 0);
  Quat qn = nlerp(q0, q1, 0.f);
  assert(std::fabs(qn.w - 1.f) < 1e-5f);
  Quat qs = slerp(q0, q1, 0.f);
  assert(std::fabs(qs.w - 1.f) < 1e-5f);

  TransformStack stack;
  stack.translate(1, 2, 3);
  assert(std::fabs(stack.matrix()._41 - 1.f) < 1e-6f);

  Matrix vp;
  vp.identity();
  Frustum fr = Frustum::from_view_proj(vp);
  assert(fr.intersects(box));

  {
    using base::LpToDp2;
    using base::transform_xy;
    using base::transform_xy_batch;
    using base::inverse_xy;
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
    assert(x == 9);
    assert(y == 85);

    const float xy_in[] = {12.f, 24.f, 10.f, 20.f};
    long xy_out[4] = {};
    transform_xy_batch(a, xy_in, xy_out);
    assert(xy_out[0] == 9 && xy_out[1] == 85);
    assert(xy_out[2] == 5);
    assert(xy_out[3] == static_cast<long>(100.f - 7));

    // Larger batch: batch path must match per-point transform_xy (scalar or
    // AVX2 when smt_render_math_simd is on).
    float big_in[16];
    long big_out[16] = {};
    long expect[16] = {};
    for (int i = 0; i < 8; ++i) {
      big_in[i * 2] = 10.f + static_cast<float>(i);
      big_in[i * 2 + 1] = 20.f + static_cast<float>(i) * 0.5f;
      transform_xy(a, big_in[i * 2], big_in[i * 2 + 1], &expect[i * 2],
                   &expect[i * 2 + 1]);
    }
    transform_xy_batch(a, big_in, big_out);
    for (int i = 0; i < 16; ++i) {
      assert(big_out[i] == expect[i]);
    }

    float ox = 0.f;
    float oy = 0.f;
    inverse_xy(a, 9, 85, &ox, &oy);
    assert(std::fabs(ox - 12.f) < 1e-3f);
    assert(std::fabs(oy - 24.f) < 1e-3f);
  }

  // Compat aliases still resolve.
  render::Vector3 rv(1, 2, 3);
  assert(std::fabs(rv.x - 1.f) < 1e-6f);

  // Golden: leftover inward IsBoxIn ≡ Frustum::intersects (outward planes).
  {
    Matrix view;
    view.view_look_at(Vector4(0, 0, 5), Vector4(0, 0, 0), Vector4(0, 1, 0));
    Matrix proj;
    proj.identity();
    proj.set_perspective(60.f, 1.333f, 0.1f, 100.f);
    const Matrix mvp = view * proj;

    float inward[6][4];
    leftover_extract_inward(mvp, inward);
    const Frustum frustum = Frustum::from_view_proj(mvp);

    const struct {
      float min_x, min_y, min_z, max_x, max_y, max_z;
    } cases[] = {
        {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f},
        {-10.f, -10.f, 50.f, -9.f, -9.f, 51.f},
        {-2.f, -2.f, -2.f, 2.f, 2.f, 2.f},
        {100.f, 100.f, 100.f, 101.f, 101.f, 101.f},
        {-0.1f, -0.1f, 0.f, 0.1f, 0.1f, 0.2f},
    };
    for (const auto& c : cases) {
      const bool old_in = leftover_is_box_in(inward, c.max_x, c.max_y, c.max_z,
                                             c.min_x, c.min_y, c.min_z);
      Aabb aabb(Vector4(c.min_x, c.min_y, c.min_z),
                Vector4(c.max_x, c.max_y, c.max_z));
      const bool neu_in = frustum.intersects(aabb);
      assert(old_in == neu_in);
    }

    constexpr int kIters = 20000;
    const auto t0 = std::chrono::steady_clock::now();
    volatile int sink = 0;
    for (int i = 0; i < kIters; ++i) {
      Frustum f = Frustum::from_view_proj(mvp);
      sink += f.intersects(box) ? 1 : 0;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double us =
        std::chrono::duration<double, std::micro>(t1 - t0).count();
    std::printf("frustum_extract_cull %d iters: %.1f us (sink=%d)\n", kIters,
                us, sink);
  }

  std::printf("math_test ok\n");
  return 0;
}

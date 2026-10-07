// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/math/simd/simd.h"

#include <algorithm>

#if defined(BASE_MATH_SIMD)
#include "base/simd/stdx.h"
#define BASE_MATH_HAVE_AVX2 1
#else
#define BASE_MATH_HAVE_AVX2 0
#endif

namespace base {

void normalize_batch(std::span<Vector3> points) {
  for (Vector3& p : points) {
    p.normalize();
  }
}

void transform_points_batch(const Matrix& m, std::span<Vector3> points) {
  for (Vector3& p : points) {
    const Vector4 out = m.transform_point(Vector4(p.x, p.y, p.z, 1.0f));
    p.set(out.x, out.y, out.z);
  }
}

namespace {

void transform_xy_batch_scalar(const LpToDp2& a, std::span<const float> xy_in,
                               std::span<long> xy_out, size_t begin_pair,
                               size_t end_pair) {
  for (size_t i = begin_pair; i < end_pair; ++i) {
    transform_xy(a, xy_in[i * 2], xy_in[i * 2 + 1], &xy_out[i * 2],
                 &xy_out[i * 2 + 1]);
  }
}

#if BASE_MATH_HAVE_AVX2
// 4 points per iteration via vir-simd (SSE-width float lanes for x/y).
void transform_xy_batch_stdx(const LpToDp2& a, const float* xy_in, long* xy_out,
                             size_t pairs) {
  namespace stdx = base::simd::stdx;
  using f4 = stdx::fixed_size_simd<float, 4>;

  const f4 wox(a.wox);
  const f4 woy(a.woy);
  const f4 vox(a.vox);
  const f4 voy(a.voy);
  const f4 scale(a.scale);
  const f4 half(0.5f);
  const f4 view_h(a.view_h);
  const bool flip = a.flip_y;

  size_t i = 0;
  for (; i + 4 <= pairs; i += 4) {
    alignas(16) float xs[4];
    alignas(16) float ys[4];
    for (int k = 0; k < 4; ++k) {
      xs[k] = xy_in[(i + static_cast<size_t>(k)) * 2u];
      ys[k] = xy_in[(i + static_cast<size_t>(k)) * 2u + 1u];
    }
    const f4 vx(xs, stdx::element_aligned);
    const f4 vy(ys, stdx::element_aligned);

    f4 xf = vox + (vx - wox) * scale + half;
    f4 yf = voy + (vy - woy) * scale + half;

    // trunc toward zero, then optional flip_y as view_h - Y.
    alignas(16) float xf_a[4];
    alignas(16) float yf_a[4];
    stdx::trunc(xf).copy_to(xf_a, stdx::element_aligned);
    stdx::trunc(yf).copy_to(yf_a, stdx::element_aligned);
    for (int k = 0; k < 4; ++k) {
      long xi = static_cast<long>(xf_a[k]);
      long yi = static_cast<long>(yf_a[k]);
      if (flip) {
        yi = static_cast<long>(a.view_h - static_cast<float>(yi));
      }
      xy_out[(i + static_cast<size_t>(k)) * 2u] = xi;
      xy_out[(i + static_cast<size_t>(k)) * 2u + 1u] = yi;
    }
    (void)view_h;
  }
  transform_xy_batch_scalar(a, std::span<const float>(xy_in, pairs * 2),
                            std::span<long>(xy_out, pairs * 2), i, pairs);
}
#endif

}  // namespace

void transform_xy_batch(const LpToDp2& a, std::span<const float> xy_in,
                        std::span<long> xy_out) {
  const size_t n = (std::min)(xy_in.size(), xy_out.size());
  const size_t pairs = n / 2;
#if BASE_MATH_HAVE_AVX2
  if (pairs >= 4) {
    transform_xy_batch_stdx(a, xy_in.data(), xy_out.data(), pairs);
    return;
  }
#endif
  transform_xy_batch_scalar(a, xy_in, xy_out, 0, pairs);
}

}  // namespace base

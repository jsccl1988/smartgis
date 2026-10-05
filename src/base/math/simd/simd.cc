// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/math/simd/simd.h"

#include <algorithm>

#if defined(BASE_MATH_SIMD) && (defined(__AVX2__) || defined(__AVX2))
#include <immintrin.h>
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
// 4 points per iteration. Matches transform_xy: +0.5 then trunc-toward-zero,
// then flip_y as static_cast<long>(view_h - Y) with Y promoted to float.
void transform_xy_batch_avx2(const LpToDp2& a, const float* xy_in, long* xy_out,
                             size_t pairs) {
  const __m128 wox = _mm_set1_ps(a.wox);
  const __m128 woy = _mm_set1_ps(a.woy);
  const __m128 vox = _mm_set1_ps(a.vox);
  const __m128 voy = _mm_set1_ps(a.voy);
  const __m128 scale = _mm_set1_ps(a.scale);
  const __m128 half = _mm_set1_ps(0.5f);
  const __m128 view_h = _mm_set1_ps(a.view_h);
  const bool flip = a.flip_y;

  size_t i = 0;
  for (; i + 4 <= pairs; i += 4) {
    const __m256 xy = _mm256_loadu_ps(xy_in + i * 2);
    alignas(32) float tmp[8];
    _mm256_store_ps(tmp, xy);
    // _mm_set_ps(e3,e2,e1,e0) → lane0=e0 … lane3=e3
    const __m128 vx = _mm_set_ps(tmp[6], tmp[4], tmp[2], tmp[0]);
    const __m128 vy = _mm_set_ps(tmp[7], tmp[5], tmp[3], tmp[1]);

    const __m128 xf =
        _mm_add_ps(_mm_add_ps(vox, _mm_mul_ps(_mm_sub_ps(vx, wox), scale)),
                   half);
    const __m128 yf =
        _mm_add_ps(_mm_add_ps(voy, _mm_mul_ps(_mm_sub_ps(vy, woy), scale)),
                   half);

    __m128i xi = _mm_cvttps_epi32(xf);
    __m128i yi = _mm_cvttps_epi32(yf);
    if (flip) {
      const __m128 y_as_f = _mm_cvtepi32_ps(yi);
      yi = _mm_cvttps_epi32(_mm_sub_ps(view_h, y_as_f));
    }

    alignas(16) int xi_s[4];
    alignas(16) int yi_s[4];
    _mm_store_si128(reinterpret_cast<__m128i*>(xi_s), xi);
    _mm_store_si128(reinterpret_cast<__m128i*>(yi_s), yi);
    for (int k = 0; k < 4; ++k) {
      xy_out[(i + static_cast<size_t>(k)) * 2u] = xi_s[k];
      xy_out[(i + static_cast<size_t>(k)) * 2u + 1u] = yi_s[k];
    }
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
    transform_xy_batch_avx2(a, xy_in.data(), xy_out.data(), pairs);
    return;
  }
#endif
  transform_xy_batch_scalar(a, xy_in, xy_out, 0, pairs);
}

}  // namespace base

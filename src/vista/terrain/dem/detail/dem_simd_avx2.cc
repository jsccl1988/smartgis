// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// AVX2-width DEM height kernels via vir-simd (C++26 std::simd candidate).
// This TU is only built with //src/base/math:math_simd_avx2 (/arch:AVX2).

#include "vista/terrain/dem/detail/dem_simd.h"

#include "base/simd/stdx.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#if defined(BASE_MATH_SIMD)

namespace vista {
namespace detail {
namespace {

namespace stdx = base::simd::stdx;

using fvec = stdx::fixed_size_simd<float, 8>;
constexpr int kW = static_cast<int>(fvec::size());

}  // namespace

bool dem_simd_avx2_compiled() { return true; }

void minmax_f32_avx2(const float* p, size_t n, float* out_min, float* out_max) {
  if (!p || !out_min || !out_max || n == 0) {
    return;
  }
  fvec vmin(p[0]);
  fvec vmax = vmin;
  size_t i = 0;
  for (; i + static_cast<size_t>(kW) <= n; i += static_cast<size_t>(kW)) {
    const fvec v(p + i, stdx::element_aligned);
    vmin = stdx::min(vmin, v);
    vmax = stdx::max(vmax, v);
  }
  float lo = stdx::hmin(vmin);
  float hi = stdx::hmax(vmax);
  for (; i < n; ++i) {
    lo = (std::min)(lo, p[i]);
    hi = (std::max)(hi, p[i]);
  }
  *out_min = lo;
  *out_max = hi;
}

void fill_lod_bilinear_row_avx2(const float* heights, int src_cols,
                                int src_rows, float fy, int w, float inv_w,
                                float cols_f, float* out) {
  if (!heights || !out || w < kW || src_cols < 1 || src_rows < 1) {
    return;
  }
  fy = std::clamp(fy, 0.f, static_cast<float>(src_rows - 1));
  const int r0 = static_cast<int>(std::floor(fy));
  const int r1 = (std::min)(r0 + 1, src_rows - 1);
  const float ty = fy - static_cast<float>(r0);
  const float* row0 =
      heights + static_cast<size_t>(r0) * static_cast<size_t>(src_cols);
  const float* row1 =
      heights + static_cast<size_t>(r1) * static_cast<size_t>(src_cols);
  const fvec one(1.f);
  const fvec ty_v(ty);
  const fvec omy = one - ty_v;
  const fvec half(0.5f);
  const fvec inv_w_v(inv_w);
  const fvec cols_v(cols_f);
  const fvec col_base([](std::size_t i) { return static_cast<float>(i); });
  const float col_max = static_cast<float>(src_cols - 1);

  for (int col = 0; col + kW <= w; col += kW) {
    const fvec col_f = fvec(static_cast<float>(col)) + col_base;
    fvec fx = (col_f + half) * inv_w_v * cols_v - half;
    fx = stdx::max(fx, fvec(0.f));
    fx = stdx::min(fx, fvec(col_max));

    alignas(32) float fx_a[kW];
    fx.copy_to(fx_a, stdx::element_aligned);

    alignas(32) float h00[kW];
    alignas(32) float h10[kW];
    alignas(32) float h01[kW];
    alignas(32) float h11[kW];
    alignas(32) float tx_a[kW];
    for (int k = 0; k < kW; ++k) {
      const float f = fx_a[k];
      const int c0 = static_cast<int>(std::floor(f));
      const int c1 = (std::min)(c0 + 1, src_cols - 1);
      tx_a[k] = f - static_cast<float>(c0);
      h00[k] = row0[c0];
      h10[k] = row0[c1];
      h01[k] = row1[c0];
      h11[k] = row1[c1];
    }
    const fvec tx(tx_a, stdx::element_aligned);
    const fvec omx = one - tx;
    const fvec top = fvec(h00, stdx::element_aligned) * omx +
                     fvec(h10, stdx::element_aligned) * tx;
    const fvec bot = fvec(h01, stdx::element_aligned) * omx +
                     fvec(h11, stdx::element_aligned) * tx;
    const fvec v = top * omy + bot * ty_v;
    v.copy_to(out + col, stdx::element_aligned);
  }
}

}  // namespace detail
}  // namespace vista

#else  // !BASE_MATH_SIMD

namespace vista {
namespace detail {

bool dem_simd_avx2_compiled() { return false; }

void minmax_f32_avx2(const float*, size_t, float*, float*) {}

void fill_lod_bilinear_row_avx2(const float*, int, int, float, int, float,
                                float, float*) {}

}  // namespace detail
}  // namespace vista

#endif  // BASE_MATH_SIMD

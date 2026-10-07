// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// AVX2-width coverage bake via vir-simd (C++26 std::simd candidate).
// Compiled with /arch:AVX2 so native_simd<float> is width 8.

#include "vista/component/map/shade/multiply_kern.h"

#include "base/simd/stdx.h"

#include <cstddef>
#include <cstdint>

// This TU is only built with //src/base/math:math_simd_avx2 (/arch:AVX2).
#if defined(BASE_MATH_SIMD)

namespace vista {
namespace detail {
namespace {

namespace stdx = base::simd::stdx;

using fvec = stdx::fixed_size_simd<float, 8>;
using ivec = stdx::fixed_size_simd<int, 8>;
constexpr int kW = static_cast<int>(fvec::size());

}  // namespace

bool multiply_avx2_compiled() { return true; }

void apply_multiply_coverage_avx2(std::uint8_t* rgba, size_t pixels,
                                  float opacity) {
  const fvec k255(255.f);
  const fvec c_r(0.299f);
  const fvec c_g(0.587f);
  const fvec c_b(0.114f);
  const fvec op(opacity);
  const fvec one_minus(1.f - opacity);
  const fvec half(0.5f);
  const fvec one(1.f);
  const ivec thresh(160);
  const ivec bytes_max(255);
  const ivec opaque(255);
  const ivec zero_i(0);

  std::uint8_t* p = rgba;
  const std::uint8_t* end = rgba + pixels * 4u;
  for (; p + static_cast<size_t>(kW) * 4u <= end; p += static_cast<size_t>(kW) * 4u) {
    alignas(32) float rf[kW];
    alignas(32) float gf[kW];
    alignas(32) float bf[kW];
    alignas(32) int ai[kW];
    for (int k = 0; k < kW; ++k) {
      rf[k] = static_cast<float>(p[k * 4 + 0]);
      gf[k] = static_cast<float>(p[k * 4 + 1]);
      bf[k] = static_cast<float>(p[k * 4 + 2]);
      ai[k] = static_cast<int>(p[k * 4 + 3]);
    }
    const fvec r = fvec(rf, stdx::element_aligned) / k255;
    const fvec g = fvec(gf, stdx::element_aligned) / k255;
    const fvec b = fvec(bf, stdx::element_aligned) / k255;
    const fvec luma = c_r * r + c_g * g + c_b * b;
    fvec m = one_minus + op * luma;
    m = stdx::max(m, fvec(0.f));
    m = stdx::min(m, one);
    ivec factor = stdx::static_simd_cast<int>(stdx::trunc(m * k255 + half));
    factor = stdx::max(factor, zero_i);
    factor = stdx::min(factor, bytes_max);

    const ivec a_lane(ai, stdx::element_aligned);
    // drop when A < 160
    const auto drop = a_lane < thresh;
    stdx::where(drop, factor) = zero_i;
    ivec alpha = opaque;
    stdx::where(drop, alpha) = zero_i;

    for (int k = 0; k < kW; ++k) {
      const int f = factor[k];
      const int a = alpha[k];
      p[k * 4 + 0] = static_cast<std::uint8_t>(f);
      p[k * 4 + 1] = static_cast<std::uint8_t>(f);
      p[k * 4 + 2] = static_cast<std::uint8_t>(f);
      p[k * 4 + 3] = static_cast<std::uint8_t>(a);
    }
  }
}

}  // namespace detail
}  // namespace vista

#else

namespace vista {
namespace detail {

bool multiply_avx2_compiled() { return false; }

void apply_multiply_coverage_avx2(std::uint8_t*, size_t, float) {}

}  // namespace detail
}  // namespace vista

#endif

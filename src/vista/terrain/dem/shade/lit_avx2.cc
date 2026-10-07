// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// AVX2-width lit / Lambert pack via vir-simd (C++26 std::simd candidate).
// This TU is only built with //src/base/math:math_simd_avx2 (/arch:AVX2).

#include "vista/terrain/dem/shade/lit_kern.h"

#include "base/simd/stdx.h"

#include <cstddef>
#include <cstdint>

#if defined(BASE_MATH_SIMD)

namespace vista {
namespace detail {
namespace {

namespace stdx = base::simd::stdx;

using fvec = stdx::fixed_size_simd<float, 8>;
using fmask = stdx::fixed_size_simd_mask<float, 8>;
constexpr int kW = static_cast<int>(fvec::size());

fvec encode_shade(fvec shade) {
  const fvec half(0.5f);
  const fvec k(1.80f);
  const fvec lo(0.08f);
  const fvec hi(1.f);
  fvec v = (shade - half) * k + half;
  v = stdx::max(v, lo);
  return stdx::min(v, hi);
}

void store_rgba8_from_rgb(uint8_t* dst, fvec r, fvec g, fvec b,
                          const fmask& ocean) {
  const fvec k255(255.f);
  const fvec half(0.5f);
  fvec ri = r * k255 + half;
  fvec gi = g * k255 + half;
  fvec bi = b * k255 + half;
  ri = stdx::max(ri, fvec(0.f));
  gi = stdx::max(gi, fvec(0.f));
  bi = stdx::max(bi, fvec(0.f));
  ri = stdx::min(ri, k255);
  gi = stdx::min(gi, k255);
  bi = stdx::min(bi, k255);
  stdx::where(ocean, ri) = fvec(0.f);
  stdx::where(ocean, gi) = fvec(0.f);
  stdx::where(ocean, bi) = fvec(0.f);

  alignas(32) float r_a[kW];
  alignas(32) float g_a[kW];
  alignas(32) float b_a[kW];
  ri.copy_to(r_a, stdx::element_aligned);
  gi.copy_to(g_a, stdx::element_aligned);
  bi.copy_to(b_a, stdx::element_aligned);
  for (int k = 0; k < kW; ++k) {
    const bool drop = ocean[k];
    dst[k * 4 + 0] = drop ? 0 : static_cast<uint8_t>(r_a[k]);
    dst[k * 4 + 1] = drop ? 0 : static_cast<uint8_t>(g_a[k]);
    dst[k * 4 + 2] = drop ? 0 : static_cast<uint8_t>(b_a[k]);
    dst[k * 4 + 3] = drop ? 0 : 255;
  }
}

}  // namespace

bool dem_lit_avx2_compiled() { return true; }

void apply_rgb_lit_mul_avx2(uint8_t* rgba, const float* shade, size_t n,
                            float bias, float scale) {
  const fvec bias_v(bias);
  const fvec scale_v(scale);
  const fvec lo(0.08f);
  const fvec hi(1.f);
  const fvec k255(255.f);
  const fvec zero(0.f);

  for (size_t i = 0; i + static_cast<size_t>(kW) <= n;
       i += static_cast<size_t>(kW)) {
    fvec s(shade + i, stdx::element_aligned);
    const auto skip = s < zero;
    s = stdx::max(s, lo);
    s = stdx::min(s, hi);
    const fvec lit = bias_v + scale_v * s;

    alignas(32) float rf[kW];
    alignas(32) float gf[kW];
    alignas(32) float bf[kW];
    for (int k = 0; k < kW; ++k) {
      uint8_t* px = rgba + (i + static_cast<size_t>(k)) * 4u;
      rf[k] = static_cast<float>(px[0]);
      gf[k] = static_cast<float>(px[1]);
      bf[k] = static_cast<float>(px[2]);
    }
    fvec r(rf, stdx::element_aligned);
    fvec g(gf, stdx::element_aligned);
    fvec b(bf, stdx::element_aligned);
    r = r * lit;
    g = g * lit;
    b = b * lit;
    r = stdx::min(stdx::max(r, zero), k255);
    g = stdx::min(stdx::max(g, zero), k255);
    b = stdx::min(stdx::max(b, zero), k255);
    r.copy_to(rf, stdx::element_aligned);
    g.copy_to(gf, stdx::element_aligned);
    b.copy_to(bf, stdx::element_aligned);
    for (int k = 0; k < kW; ++k) {
      if (skip[k]) {
        continue;
      }
      uint8_t* px = rgba + (i + static_cast<size_t>(k)) * 4u;
      px[0] = static_cast<uint8_t>(rf[k] + 0.5f);
      px[1] = static_cast<uint8_t>(gf[k] + 0.5f);
      px[2] = static_cast<uint8_t>(bf[k] + 0.5f);
    }
  }
}

void pack_lambert_from_shade_avx2(const float* shade, const float* heights,
                                  uint8_t* rgba, size_t n, float sr, float sg,
                                  float sb, float hr, float hg, float hb,
                                  bool contrast) {
  const fvec sr_v(sr);
  const fvec sg_v(sg);
  const fvec sb_v(sb);
  const fvec d_r(hr - sr);
  const fvec d_g(hg - sg);
  const fvec d_b(hb - sb);
  const fvec one(1.f);
  const fvec lo(0.08f);

  for (size_t i = 0; i + static_cast<size_t>(kW) <= n;
       i += static_cast<size_t>(kW)) {
    fvec s(shade + i, stdx::element_aligned);
    fmask ocean(false);
    if (heights) {
      const fvec h(heights + i, stdx::element_aligned);
      ocean = h <= one;
    }
    if (contrast) {
      s = encode_shade(s);
    } else {
      s = stdx::max(s, lo);
      s = stdx::min(s, one);
    }
    const fvec r = sr_v + d_r * s;
    const fvec g = sg_v + d_g * s;
    const fvec b = sb_v + d_b * s;
    store_rgba8_from_rgb(rgba + i * 4u, r, g, b, ocean);
  }
}

}  // namespace detail
}  // namespace vista

#else  // !BASE_MATH_SIMD

namespace vista {
namespace detail {

bool dem_lit_avx2_compiled() { return false; }

void apply_rgb_lit_mul_avx2(uint8_t*, const float*, size_t, float, float) {}

void pack_lambert_from_shade_avx2(const float*, const float*, uint8_t*, size_t,
                                  float, float, float, float, float, float,
                                  bool) {}

}  // namespace detail
}  // namespace vista

#endif  // BASE_MATH_SIMD

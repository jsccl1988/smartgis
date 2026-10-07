// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// AVX2 coverage bake. Compiled with /arch:AVX2 (or -mavx2) so the scalar
// translation unit can stay free of VEX instructions and still run on
// CPUs that fail the runtime check.

#include "vista/component/map/shade/multiply_kern.h"

#include <cstddef>
#include <cstdint>

#if defined(__AVX2__)

#include <immintrin.h>

namespace vista {
namespace detail {
namespace {

__m128i pack_channel(const __m256i px, const __m256i shuf) {
  // Shuffle is per 128-bit lane: lane 0 holds pixels 0..3, lane 1 holds 4..7.
  // Low 4 bytes of each lane are the selected channel. unpacklo joins them
  // into 8 contiguous bytes for cvtepu8.
  const __m256i gathered = _mm256_shuffle_epi8(px, shuf);
  const __m128i lo = _mm256_castsi256_si128(gathered);
  const __m128i hi = _mm256_extracti128_si256(gathered, 1);
  return _mm_unpacklo_epi32(lo, hi);
}

__m256i channel_shuf(int b0, int b1, int b2, int b3) {
  return _mm256_setr_epi8(
      static_cast<char>(b0), static_cast<char>(b1), static_cast<char>(b2),
      static_cast<char>(b3), -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      static_cast<char>(b0), static_cast<char>(b1), static_cast<char>(b2),
      static_cast<char>(b3), -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
}

}  // namespace

bool multiply_avx2_compiled() { return true; }

void apply_multiply_coverage_avx2(std::uint8_t* rgba, size_t pixels,
                                  float opacity) {
  const __m256 k255 = _mm256_set1_ps(255.f);
  const __m256 c_r = _mm256_set1_ps(0.299f);
  const __m256 c_g = _mm256_set1_ps(0.587f);
  const __m256 c_b = _mm256_set1_ps(0.114f);
  const __m256 op = _mm256_set1_ps(opacity);
  const __m256 one_minus = _mm256_set1_ps(1.f - opacity);
  const __m256 half = _mm256_set1_ps(0.5f);
  const __m256 one = _mm256_set1_ps(1.f);
  const __m256i shuf_r = channel_shuf(0, 4, 8, 12);
  const __m256i shuf_g = channel_shuf(1, 5, 9, 13);
  const __m256i shuf_b = channel_shuf(2, 6, 10, 14);
  const __m256i shuf_a = channel_shuf(3, 7, 11, 15);
  const __m256i thresh = _mm256_set1_epi32(160);
  const __m256i bytes_max = _mm256_set1_epi32(255);
  const __m256i opaque = _mm256_set1_epi32(255);

  std::uint8_t* p = rgba;
  const std::uint8_t* end = rgba + pixels * 4u;
  for (; p + 32 <= end; p += 32) {
    const __m256i px =
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
    const __m128i rb = pack_channel(px, shuf_r);
    const __m128i gb = pack_channel(px, shuf_g);
    const __m128i bb = pack_channel(px, shuf_b);
    const __m128i ab = pack_channel(px, shuf_a);

    // Same order as the scalar loop: byte/255, then the luma weights.
    const __m256 rf = _mm256_div_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(rb)),
                                    k255);
    const __m256 gf = _mm256_div_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(gb)),
                                    k255);
    const __m256 bf = _mm256_div_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(bb)),
                                    k255);
    const __m256 luma = _mm256_add_ps(
        _mm256_mul_ps(c_r, rf),
        _mm256_add_ps(_mm256_mul_ps(c_g, gf), _mm256_mul_ps(c_b, bf)));
    __m256 m = _mm256_add_ps(one_minus, _mm256_mul_ps(op, luma));
    m = _mm256_max_ps(m, _mm256_setzero_ps());
    m = _mm256_min_ps(m, one);
    __m256i factor = _mm256_cvttps_epi32(
        _mm256_add_ps(_mm256_mul_ps(m, k255), half));
    factor = _mm256_max_epi32(factor, _mm256_setzero_si256());
    factor = _mm256_min_epi32(factor, bytes_max);

    // Scalar test is A < 160. cmpgt(160, A) is that predicate.
    const __m256i drop =
        _mm256_cmpgt_epi32(thresh, _mm256_cvtepu8_epi32(ab));
    factor = _mm256_andnot_si256(drop, factor);
    const __m256i alpha = _mm256_andnot_si256(drop, opaque);

    // Little-endian epi32 stores R,G,B,A for each pixel.
    __m256i out = factor;
    out = _mm256_or_si256(out, _mm256_slli_epi32(factor, 8));
    out = _mm256_or_si256(out, _mm256_slli_epi32(factor, 16));
    out = _mm256_or_si256(out, _mm256_slli_epi32(alpha, 24));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), out);
  }
  _mm256_zeroupper();
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

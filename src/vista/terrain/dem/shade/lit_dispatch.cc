// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/shade/lit_kern.h"

#include "vista/terrain/dem/bake/bake_parallel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(__i386__)
#define VISTA_DEM_LIT_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace vista {
namespace detail {
namespace {

float encode_shade(float shade) {
  return std::clamp((shade - 0.5f) * 1.80f + 0.5f, 0.08f, 1.f);
}

#if defined(VISTA_DEM_LIT_X86)
bool cpu_has_avx2() {
#if defined(_MSC_VER)
  int cpu[4] = {};
  __cpuid(cpu, 1);
  const bool osxsave = (cpu[2] & (1 << 27)) != 0;
  const bool avx = (cpu[2] & (1 << 28)) != 0;
  if (!osxsave || !avx) {
    return false;
  }
  const unsigned long long xcr0 = _xgetbv(0);
  if ((xcr0 & 0x6ull) != 0x6ull) {
    return false;
  }
  __cpuidex(cpu, 7, 0);
  return (cpu[1] & (1 << 5)) != 0;
#else
  unsigned int eax = 0;
  unsigned int ebx = 0;
  unsigned int ecx = 0;
  unsigned int edx = 0;
  if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    return false;
  }
  const bool osxsave = (ecx & (1u << 27)) != 0;
  const bool avx = (ecx & (1u << 28)) != 0;
  if (!osxsave || !avx) {
    return false;
  }
  unsigned int xcr_lo = 0;
  unsigned int xcr_hi = 0;
  __asm__ volatile("xgetbv" : "=a"(xcr_lo), "=d"(xcr_hi) : "c"(0));
  const unsigned long long xcr0 =
      (static_cast<unsigned long long>(xcr_hi) << 32) | xcr_lo;
  if ((xcr0 & 0x6ull) != 0x6ull) {
    return false;
  }
  if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
    return false;
  }
  return (ebx & (1u << 5)) != 0;
#endif
}
#endif

}  // namespace

bool dem_lit_avx2_runtime() {
  if (!bake_simd_wanted()) {
    return false;
  }
#if defined(VISTA_DEM_LIT_X86)
  static const bool enabled = cpu_has_avx2() && dem_lit_avx2_compiled();
  return enabled;
#else
  return false;
#endif
}

void apply_rgb_lit_mul_scalar(uint8_t* rgba, const float* shade, size_t n,
                              float bias, float scale) {
  if (!rgba || !shade || n == 0) {
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    const float s = shade[i];
    if (s < 0.f) {
      continue;
    }
    const float lit =
        bias + scale * std::clamp(s, 0.08f, 1.f);
    uint8_t* px = rgba + i * 4u;
    px[0] = static_cast<uint8_t>(
        std::clamp(static_cast<float>(px[0]) * lit, 0.f, 255.f) + 0.5f);
    px[1] = static_cast<uint8_t>(
        std::clamp(static_cast<float>(px[1]) * lit, 0.f, 255.f) + 0.5f);
    px[2] = static_cast<uint8_t>(
        std::clamp(static_cast<float>(px[2]) * lit, 0.f, 255.f) + 0.5f);
  }
}

void apply_rgb_lit_mul(uint8_t* rgba, const float* shade, size_t n, float bias,
                       float scale) {
  if (!rgba || !shade || n == 0) {
    return;
  }
  size_t i = 0;
  if (n >= 8 && dem_lit_avx2_runtime()) {
    const size_t blocks = n / 8u;
    const size_t simd_n = blocks * 8u;
    apply_rgb_lit_mul_avx2(rgba, shade, simd_n, bias, scale);
    i = simd_n;
  }
  if (i < n) {
    apply_rgb_lit_mul_scalar(rgba + i * 4u, shade + i, n - i, bias, scale);
  }
}

void pack_lambert_from_shade_scalar(const float* shade, const float* heights,
                                    uint8_t* rgba, size_t n, float sr, float sg,
                                    float sb, float hr, float hg, float hb,
                                    bool contrast) {
  if (!shade || !rgba || n == 0) {
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    uint8_t* px = rgba + i * 4u;
    if (heights && heights[i] <= 1.f) {
      px[0] = 0;
      px[1] = 0;
      px[2] = 0;
      px[3] = 0;
      continue;
    }
    float shade_v = shade[i];
    if (contrast) {
      shade_v = encode_shade(shade_v);
    } else {
      shade_v = std::clamp(shade_v, 0.08f, 1.f);
    }
    px[0] = static_cast<uint8_t>(
        std::clamp(sr + (hr - sr) * shade_v, 0.f, 1.f) * 255.f + 0.5f);
    px[1] = static_cast<uint8_t>(
        std::clamp(sg + (hg - sg) * shade_v, 0.f, 1.f) * 255.f + 0.5f);
    px[2] = static_cast<uint8_t>(
        std::clamp(sb + (hb - sb) * shade_v, 0.f, 1.f) * 255.f + 0.5f);
    px[3] = 255;
  }
}

void pack_lambert_from_shade(const float* shade, const float* heights,
                             uint8_t* rgba, size_t n, float sr, float sg,
                             float sb, float hr, float hg, float hb,
                             bool contrast) {
  if (!shade || !rgba || n == 0) {
    return;
  }
  size_t i = 0;
  if (n >= 8 && dem_lit_avx2_runtime()) {
    const size_t blocks = n / 8u;
    const size_t simd_n = blocks * 8u;
    pack_lambert_from_shade_avx2(shade, heights, rgba, simd_n, sr, sg, sb, hr,
                                 hg, hb, contrast);
    i = simd_n;
  }
  if (i < n) {
    pack_lambert_from_shade_scalar(shade + i, heights ? heights + i : nullptr,
                                   rgba + i * 4u, n - i, sr, sg, sb, hr, hg, hb,
                                   contrast);
  }
}

}  // namespace detail
}  // namespace vista

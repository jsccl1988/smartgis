// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/map/shade/multiply.h"

#include "vista/component/map/shade/multiply_kern.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#define VISTA_MULTIPLY_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace vista {
namespace detail {

void apply_multiply_coverage_scalar(std::uint8_t* rgba, size_t pixels,
                                    float opacity) {
  const float op = opacity;
  for (size_t i = 0; i < pixels; ++i) {
    std::uint8_t* px = rgba + i * 4u;
    if (px[3] < 160) {
      px[0] = 0;
      px[1] = 0;
      px[2] = 0;
      px[3] = 0;
      continue;
    }
    const float r = static_cast<float>(px[0]) / 255.f;
    const float g = static_cast<float>(px[1]) / 255.f;
    const float b = static_cast<float>(px[2]) / 255.f;
    const float luma = 0.299f * r + 0.587f * g + 0.114f * b;
    const float m = (1.f - op) + op * luma;
    // Store the factor in every channel so a later dst *= src blend is
    // dst * ((1 - opacity) + opacity * luma), not a second luma multiply.
    const float clamped = (std::max)(0.f, (std::min)(1.f, m));
    const auto factor = static_cast<std::uint8_t>(clamped * 255.f + 0.5f);
    px[0] = factor;
    px[1] = factor;
    px[2] = factor;
    px[3] = 255;
  }
}

}  // namespace detail

namespace {

#if defined(VISTA_MULTIPLY_X86)
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

namespace detail {

bool multiply_avx2_runtime() {
#if defined(VISTA_MULTIPLY_X86)
  static const bool enabled = cpu_has_avx2() && multiply_avx2_compiled();
  return enabled;
#else
  return false;
#endif
}

}  // namespace detail

void apply_multiply_coverage(std::span<std::uint8_t> rgba, float opacity) {
  if (rgba.empty() || (rgba.size() % 4) != 0) {
    return;
  }
  const float op = (std::max)(0.f, (std::min)(1.f, opacity));
  std::uint8_t* p = rgba.data();
  size_t pixels = rgba.size() / 4u;
  if (pixels >= 8 && detail::multiply_avx2_runtime()) {
    const size_t blocks = pixels / 8u;
    const size_t simd_pixels = blocks * 8u;
    detail::apply_multiply_coverage_avx2(p, simd_pixels, op);
    p += simd_pixels * 4u;
    pixels -= simd_pixels;
  }
  detail::apply_multiply_coverage_scalar(p, pixels, op);
}

}  // namespace vista

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/detail/dem_simd.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/terrain/dem/bake/bake_parallel.h"

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(__i386__)
#define VISTA_DEM_SIMD_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace vista {
namespace detail {
namespace {

#if defined(VISTA_DEM_SIMD_X86)
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

float sample_bilinear(const float* heights, int cols, int rows, float col_f,
                      float row_f) {
  col_f = std::clamp(col_f, 0.f, static_cast<float>(cols - 1));
  row_f = std::clamp(row_f, 0.f, static_cast<float>(rows - 1));
  const int c0 = static_cast<int>(std::floor(col_f));
  const int r0 = static_cast<int>(std::floor(row_f));
  const int c1 = (std::min)(c0 + 1, cols - 1);
  const int r1 = (std::min)(r0 + 1, rows - 1);
  const float tx = col_f - static_cast<float>(c0);
  const float ty = row_f - static_cast<float>(r0);
  const float* row0 = heights + static_cast<size_t>(r0) * static_cast<size_t>(cols);
  const float* row1 = heights + static_cast<size_t>(r1) * static_cast<size_t>(cols);
  const float h0 = row0[c0] * (1.f - tx) + row0[c1] * tx;
  const float h1 = row1[c0] * (1.f - tx) + row1[c1] * tx;
  return h0 * (1.f - ty) + h1 * ty;
}

}  // namespace

bool dem_simd_avx2_runtime() {
  if (!bake_simd_wanted()) {
    return false;
  }
#if defined(VISTA_DEM_SIMD_X86)
  static const bool enabled = cpu_has_avx2() && dem_simd_avx2_compiled();
  return enabled;
#else
  return false;
#endif
}

void minmax_f32_scalar(const float* p, size_t n, float* out_min,
                       float* out_max) {
  if (!p || !out_min || !out_max || n == 0) {
    return;
  }
  float lo = p[0];
  float hi = p[0];
  for (size_t i = 1; i < n; ++i) {
    lo = (std::min)(lo, p[i]);
    hi = (std::max)(hi, p[i]);
  }
  *out_min = lo;
  *out_max = hi;
}

void minmax_f32(const float* p, size_t n, float* out_min, float* out_max) {
  if (!p || !out_min || !out_max || n == 0) {
    return;
  }
  if (n >= 8 && dem_simd_avx2_runtime()) {
    minmax_f32_avx2(p, n, out_min, out_max);
    return;
  }
  minmax_f32_scalar(p, n, out_min, out_max);
}

void fill_lod_bilinear_row_scalar(const float* heights, int src_cols,
                                  int src_rows, float fy, int w, float inv_w,
                                  float cols_f, float* out) {
  if (!heights || !out || w < 1 || src_cols < 1 || src_rows < 1) {
    return;
  }
  for (int col = 0; col < w; ++col) {
    const float fx =
        (static_cast<float>(col) + 0.5f) * inv_w * cols_f - 0.5f;
    out[col] = sample_bilinear(heights, src_cols, src_rows, fx, fy);
  }
}

void fill_lod_bilinear_row(const float* heights, int src_cols, int src_rows,
                           float fy, int w, float inv_w, float cols_f,
                           float* out) {
  if (!heights || !out || w < 1) {
    return;
  }
  int col = 0;
  if (w >= 8 && dem_simd_avx2_runtime()) {
    const int simd_n = (w / 8) * 8;
    fill_lod_bilinear_row_avx2(heights, src_cols, src_rows, fy, simd_n, inv_w,
                               cols_f, out);
    col = simd_n;
  }
  for (; col < w; ++col) {
    const float fx =
        (static_cast<float>(col) + 0.5f) * inv_w * cols_f - 0.5f;
    out[col] = sample_bilinear(heights, src_cols, src_rows, fx, fy);
  }
}

void fill_lod_bilinear_grid(const float* heights, int src_cols, int src_rows,
                            int w, int h, float* lod) {
  if (!heights || !lod || w < 1 || h < 1 || src_cols < 1 || src_rows < 1) {
    return;
  }
  const float cols_f = static_cast<float>(src_cols);
  const float rows_f = static_cast<float>(src_rows);
  const float inv_w = 1.f / static_cast<float>(w);
  const float inv_h = 1.f / static_cast<float>(h);
  auto fill_row = [&](int row) {
    const float fy =
        (static_cast<float>(row) + 0.5f) * inv_h * rows_f - 0.5f;
    float* rowp = lod + static_cast<size_t>(row) * static_cast<size_t>(w);
    fill_lod_bilinear_row(heights, src_cols, src_rows, fy, w, inv_w, cols_f,
                          rowp);
  };
  if (bake_rows_should_parallel(w, h)) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, h, fill_row);
  } else {
    for (int row = 0; row < h; ++row) {
      fill_row(row);
    }
  }
}

}  // namespace detail
}  // namespace vista

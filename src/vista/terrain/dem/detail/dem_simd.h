// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// DEM height-grid SIMD helpers. Scalar always works; AVX2-width kernels in
// dem_simd_avx2.cc use vir-simd (C++26 std::simd candidate) with /arch:AVX2,
// gated by a runtime CPUID check.

#ifndef VISTA_TERRAIN_DEM_DETAIL_DEM_SIMD_H_
#define VISTA_TERRAIN_DEM_DETAIL_DEM_SIMD_H_

#include <cstddef>

namespace vista {
namespace detail {

// True when the AVX2 TU is linked and the CPU/OS can run YMM code.
bool dem_simd_avx2_runtime();
bool dem_simd_avx2_compiled();

// Min/max over a contiguous float buffer (n >= 1).
void minmax_f32(const float* p, size_t n, float* out_min, float* out_max);

void minmax_f32_scalar(const float* p, size_t n, float* out_min,
                       float* out_max);
void minmax_f32_avx2(const float* p, size_t n, float* out_min, float* out_max);

// Bilinear downsample from a row-major height grid into |lod| (w*h).
// |src_cols|/|src_rows| describe |heights|; |w|/|h| is the LOD size.
void fill_lod_bilinear_grid(const float* heights, int src_cols, int src_rows,
                            int w, int h, float* lod);

// One LOD row: |fy| is the fractional source row; writes |w| floats to |out|.
void fill_lod_bilinear_row(const float* heights, int src_cols, int src_rows,
                           float fy, int w, float inv_w, float cols_f,
                           float* out);

void fill_lod_bilinear_row_scalar(const float* heights, int src_cols,
                                  int src_rows, float fy, int w, float inv_w,
                                  float cols_f, float* out);
void fill_lod_bilinear_row_avx2(const float* heights, int src_cols,
                                int src_rows, float fy, int w, float inv_w,
                                float cols_f, float* out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_DETAIL_DEM_SIMD_H_

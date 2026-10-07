// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// SIMD helpers for mesh tessellation hot loops. Scalar always works; AVX2-width
// kernels in mesh_simd_avx2.cc use vir-simd (C++26 std::simd candidate) and
// /arch:AVX2, gated by a runtime CPUID check so other mesh TUs stay portable.

#ifndef VISTA_MESH_DETAIL_MESH_SIMD_H_
#define VISTA_MESH_DETAIL_MESH_SIMD_H_

#include <cstddef>
#include <cstdint>

#include "vista/mesh/detail/mesh_types.h"
#include "vista/mesh/tessellate.h"

namespace vista {
namespace detail {

// True when the AVX2 TU is linked and the CPU/OS can run YMM code.
bool mesh_simd_avx2_runtime();

// N/S/E/W extrema indices over SoA xy. n must be >= 1.
void find_xy_extrema(const double* x, const double* y, int n, int* i_n,
                     int* i_s, int* i_e, int* i_w);

// RDP farthest vertex in (lo, hi) from segment (x[lo],y[lo])-(x[hi],y[hi]).
// Returns -1 when no interior point exceeds tol2.
int rdp_farthest_index(const double* x, const double* y, int lo, int hi,
                       double tol2);

// In-place unit normalize of direction vectors (zero-length → {0,0}).
void normalize_dirs_batch(Vec2* dirs, size_t count);

// Sum of consecutive hypot lengths for SoA xy (n points → n-1 edges).
double path_length_xy(const double* x, const double* y, int n);

// Axis-aligned bounds of interleaved float xyz (stride 3).
void aabb_xyz_f32(const float* xyz, size_t point_count, float* min_xyz,
                  float* max_xyz);

// Append |src| into |dst|, remapping indices by the current vertex base.
void append_mesh(TessMesh& dst, const TessMesh& src);

// Parallel batch thresholds (pool overhead below these is not worth it).
// Geom fan-out is already parallel in layout tess_jobs; only large batch
// tessellate_geoms calls should nest another parallel_for.
inline constexpr size_t kMeshParallelMinGeoms = 512;
// Point-cloud cube emit: Release matrix shows parallel wins above ~2k.
inline constexpr size_t kMeshParallelMinCloudPoints = 2048;

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MESH_DETAIL_MESH_SIMD_H_

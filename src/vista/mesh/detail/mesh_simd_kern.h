// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Internal kernels: scalar always; AVX2 only when the dedicated TU is built.

#ifndef VISTA_MESH_DETAIL_MESH_SIMD_KERN_H_
#define VISTA_MESH_DETAIL_MESH_SIMD_KERN_H_

#include <cstddef>

#include "vista/mesh/detail/mesh_types.h"

namespace vista {
namespace detail {

bool mesh_simd_avx2_compiled();

void find_xy_extrema_scalar(const double* x, const double* y, int n, int* i_n,
                            int* i_s, int* i_e, int* i_w);
void find_xy_extrema_avx2(const double* x, const double* y, int n, int* i_n,
                          int* i_s, int* i_e, int* i_w);

int rdp_farthest_index_scalar(const double* x, const double* y, int lo, int hi,
                              double tol2);
int rdp_farthest_index_avx2(const double* x, const double* y, int lo, int hi,
                            double tol2);

void normalize_dirs_batch_scalar(Vec2* dirs, size_t count);
void normalize_dirs_batch_avx2(Vec2* dirs, size_t count);

double path_length_xy_scalar(const double* x, const double* y, int n);
double path_length_xy_avx2(const double* x, const double* y, int n);

void aabb_xyz_f32_scalar(const float* xyz, size_t point_count, float* min_xyz,
                         float* max_xyz);
void aabb_xyz_f32_avx2(const float* xyz, size_t point_count, float* min_xyz,
                       float* max_xyz);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_MESH_DETAIL_MESH_SIMD_KERN_H_

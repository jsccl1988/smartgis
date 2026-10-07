// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// AVX2-width mesh kernels via vir-simd (C++26 std::simd candidate).
// Compiled with /arch:AVX2 so native_simd<double> is width 4 and
// native_simd<float> is width 8. Scalar mesh TUs stay free of that flag.

#include "vista/mesh/detail/mesh_simd_kern.h"

#include "base/simd/stdx.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

// This TU is only built with //src/base/math:math_simd_avx2 (/arch:AVX2).
#if defined(BASE_MATH_SIMD)

namespace vista {
namespace detail {
namespace {

namespace stdx = base::simd::stdx;

// Explicit AVX2 widths (not native_simd): MSVC may omit __AVX2__ even with
// /arch:AVX2 when other configs rewrite defines; fixed_size stays stable.
using dvec = stdx::fixed_size_simd<double, 4>;
using fvec = stdx::fixed_size_simd<float, 8>;

constexpr int kD = static_cast<int>(dvec::size());
constexpr int kF = static_cast<int>(fvec::size());

}  // namespace

bool mesh_simd_avx2_compiled() { return true; }

void find_xy_extrema_avx2(const double* x, const double* y, int n, int* i_n,
                          int* i_s, int* i_e, int* i_w) {
  int north = 0;
  int south = 0;
  int east = 0;
  int west = 0;
  double best_n = y[0];
  double best_s = y[0];
  double best_e = x[0];
  double best_w = x[0];

  int i = 1;
  for (; i + kD <= n; i += kD) {
    const dvec vx(x + i, stdx::element_aligned);
    const dvec vy(y + i, stdx::element_aligned);
    const double yn = stdx::hmax(vy);
    const double ys = stdx::hmin(vy);
    const double xe = stdx::hmax(vx);
    const double xw = stdx::hmin(vx);
    if (yn > best_n || ys < best_s || xe > best_e || xw < best_w) {
      for (int k = 0; k < kD; ++k) {
        const int idx = i + k;
        if (vy[k] > best_n) {
          best_n = vy[k];
          north = idx;
        }
        if (vy[k] < best_s) {
          best_s = vy[k];
          south = idx;
        }
        if (vx[k] > best_e) {
          best_e = vx[k];
          east = idx;
        }
        if (vx[k] < best_w) {
          best_w = vx[k];
          west = idx;
        }
      }
    }
  }
  for (; i < n; ++i) {
    if (y[i] > best_n) {
      best_n = y[i];
      north = i;
    }
    if (y[i] < best_s) {
      best_s = y[i];
      south = i;
    }
    if (x[i] > best_e) {
      best_e = x[i];
      east = i;
    }
    if (x[i] < best_w) {
      best_w = x[i];
      west = i;
    }
  }
  *i_n = north;
  *i_s = south;
  *i_e = east;
  *i_w = west;
}

int rdp_farthest_index_avx2(const double* x, const double* y, int lo, int hi,
                            double tol2) {
  if (hi <= lo + 1) {
    return -1;
  }
  const double ax = x[lo];
  const double ay = y[lo];
  const double bx = x[hi];
  const double by = y[hi];
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len2 = dx * dx + dy * dy;

  int farthest = -1;
  double best = tol2;

  const dvec vax(ax);
  const dvec vay(ay);
  const dvec vdx(dx);
  const dvec vdy(dy);
  const dvec vlen2(len2);
  const bool degenerate = len2 <= kEps;

  int i = lo + 1;
  for (; i + kD <= hi; i += kD) {
    const dvec px(x + i, stdx::element_aligned);
    const dvec py(y + i, stdx::element_aligned);
    dvec d2;
    if (degenerate) {
      const dvec ex = px - vax;
      const dvec ey = py - vay;
      d2 = ex * ex + ey * ey;
    } else {
      const dvec t = ((px - vax) * vdx + (py - vay) * vdy) / vlen2;
      const dvec qx = vax + t * vdx;
      const dvec qy = vay + t * vdy;
      const dvec ex = px - qx;
      const dvec ey = py - qy;
      d2 = ex * ex + ey * ey;
    }
    const double chunk_best = stdx::hmax(d2);
    if (chunk_best > best) {
      for (int k = 0; k < kD; ++k) {
        if (d2[k] > best) {
          best = d2[k];
          farthest = i + k;
        }
      }
    }
  }
  for (; i < hi; ++i) {
    const double px = x[i];
    const double py = y[i];
    double d2;
    if (degenerate) {
      const double ex = px - ax;
      const double ey = py - ay;
      d2 = ex * ex + ey * ey;
    } else {
      const double t = ((px - ax) * dx + (py - ay) * dy) / len2;
      const double qx = ax + t * dx;
      const double qy = ay + t * dy;
      const double ex = px - qx;
      const double ey = py - qy;
      d2 = ex * ex + ey * ey;
    }
    if (d2 > best) {
      best = d2;
      farthest = i;
    }
  }
  return farthest;
}

void normalize_dirs_batch_avx2(Vec2* dirs, size_t count) {
  // AoS [x0,y0,x1,y1] fills one native double simd (width 4 under AVX2).
  size_t i = 0;
  for (; i + 2 <= count; i += 2) {
    dvec v(reinterpret_cast<const double*>(&dirs[i]), stdx::element_aligned);
    const double l0 = std::hypot(v[0], v[1]);
    const double l1 = std::hypot(v[2], v[3]);
    dvec scale(0.0);
    if (l0 >= kEps) {
      scale[0] = 1.0 / l0;
      scale[1] = 1.0 / l0;
    }
    if (l1 >= kEps) {
      scale[2] = 1.0 / l1;
      scale[3] = 1.0 / l1;
    }
    (v * scale).copy_to(reinterpret_cast<double*>(&dirs[i]),
                        stdx::element_aligned);
  }
  for (; i < count; ++i) {
    dirs[i] = vec_normalize(dirs[i]);
  }
}

double path_length_xy_avx2(const double* x, const double* y, int n) {
  double sum = 0;
  int i = 0;
  for (; i + kD < n; i += kD) {
    const dvec x0(x + i, stdx::element_aligned);
    const dvec y0(y + i, stdx::element_aligned);
    const dvec x1(x + i + 1, stdx::element_aligned);
    const dvec y1(y + i + 1, stdx::element_aligned);
    const dvec dx = x1 - x0;
    const dvec dy = y1 - y0;
    const dvec len = stdx::sqrt(dx * dx + dy * dy);
    for (int k = 0; k < kD; ++k) {
      sum += len[k];
    }
  }
  for (; i + 1 < n; ++i) {
    const double dx = x[i + 1] - x[i];
    const double dy = y[i + 1] - y[i];
    sum += std::sqrt(dx * dx + dy * dy);
  }
  return sum;
}

void aabb_xyz_f32_avx2(const float* xyz, size_t point_count, float* min_xyz,
                       float* max_xyz) {
  float min_x = xyz[0];
  float min_y = xyz[1];
  float min_z = xyz[2];
  float max_x = min_x;
  float max_y = min_y;
  float max_z = min_z;
  size_t i = 1;
  // Batch over points using separate SoA temps of native float width.
  alignas(32) float xs[kF];
  alignas(32) float ys[kF];
  alignas(32) float zs[kF];
  for (; i + static_cast<size_t>(kF) <= point_count;
       i += static_cast<size_t>(kF)) {
    for (int k = 0; k < kF; ++k) {
      const size_t p = i + static_cast<size_t>(k);
      xs[k] = xyz[p * 3u];
      ys[k] = xyz[p * 3u + 1u];
      zs[k] = xyz[p * 3u + 2u];
    }
    const fvec vx(xs, stdx::element_aligned);
    const fvec vy(ys, stdx::element_aligned);
    const fvec vz(zs, stdx::element_aligned);
    min_x = (std::min)(min_x, stdx::hmin(vx));
    min_y = (std::min)(min_y, stdx::hmin(vy));
    min_z = (std::min)(min_z, stdx::hmin(vz));
    max_x = (std::max)(max_x, stdx::hmax(vx));
    max_y = (std::max)(max_y, stdx::hmax(vy));
    max_z = (std::max)(max_z, stdx::hmax(vz));
  }
  for (; i < point_count; ++i) {
    const float x = xyz[i * 3];
    const float y = xyz[i * 3 + 1];
    const float z = xyz[i * 3 + 2];
    min_x = (std::min)(min_x, x);
    min_y = (std::min)(min_y, y);
    min_z = (std::min)(min_z, z);
    max_x = (std::max)(max_x, x);
    max_y = (std::max)(max_y, y);
    max_z = (std::max)(max_z, z);
  }
  min_xyz[0] = min_x;
  min_xyz[1] = min_y;
  min_xyz[2] = min_z;
  max_xyz[0] = max_x;
  max_xyz[1] = max_y;
  max_xyz[2] = max_z;
}

}  // namespace detail
}  // namespace vista

#else

namespace vista {
namespace detail {

bool mesh_simd_avx2_compiled() { return false; }

void find_xy_extrema_avx2(const double* x, const double* y, int n, int* i_n,
                          int* i_s, int* i_e, int* i_w) {
  find_xy_extrema_scalar(x, y, n, i_n, i_s, i_e, i_w);
}

int rdp_farthest_index_avx2(const double* x, const double* y, int lo, int hi,
                            double tol2) {
  return rdp_farthest_index_scalar(x, y, lo, hi, tol2);
}

void normalize_dirs_batch_avx2(Vec2* dirs, size_t count) {
  normalize_dirs_batch_scalar(dirs, count);
}

double path_length_xy_avx2(const double* x, const double* y, int n) {
  return path_length_xy_scalar(x, y, n);
}

void aabb_xyz_f32_avx2(const float* xyz, size_t point_count, float* min_xyz,
                       float* max_xyz) {
  aabb_xyz_f32_scalar(xyz, point_count, min_xyz, max_xyz);
}

}  // namespace detail
}  // namespace vista

#endif

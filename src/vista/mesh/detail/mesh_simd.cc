// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/mesh/detail/mesh_simd.h"

#include "vista/mesh/detail/mesh_append.h"
#include "vista/mesh/detail/mesh_simd_kern.h"

#include <cmath>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(__i386__)
#define VISTA_MESH_SIMD_X86 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace vista {
namespace detail {
namespace {

#if defined(VISTA_MESH_SIMD_X86)
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

bool mesh_simd_avx2_runtime() {
#if defined(VISTA_MESH_SIMD_X86)
  static const bool enabled = cpu_has_avx2() && mesh_simd_avx2_compiled();
  return enabled;
#else
  return false;
#endif
}

void find_xy_extrema_scalar(const double* x, const double* y, int n, int* i_n,
                            int* i_s, int* i_e, int* i_w) {
  int north = 0;
  int south = 0;
  int east = 0;
  int west = 0;
  for (int i = 1; i < n; ++i) {
    if (y[i] > y[north]) {
      north = i;
    }
    if (y[i] < y[south]) {
      south = i;
    }
    if (x[i] > x[east]) {
      east = i;
    }
    if (x[i] < x[west]) {
      west = i;
    }
  }
  *i_n = north;
  *i_s = south;
  *i_e = east;
  *i_w = west;
}

int rdp_farthest_index_scalar(const double* x, const double* y, int lo, int hi,
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
  for (int i = lo + 1; i < hi; ++i) {
    const double px = x[i];
    const double py = y[i];
    double d2;
    if (len2 <= kEps) {
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

void normalize_dirs_batch_scalar(Vec2* dirs, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    dirs[i] = vec_normalize(dirs[i]);
  }
}

double path_length_xy_scalar(const double* x, const double* y, int n) {
  double sum = 0;
  for (int i = 0; i + 1 < n; ++i) {
    const double dx = x[i + 1] - x[i];
    const double dy = y[i + 1] - y[i];
    sum += std::sqrt(dx * dx + dy * dy);
  }
  return sum;
}

void aabb_xyz_f32_scalar(const float* xyz, size_t point_count, float* min_xyz,
                         float* max_xyz) {
  float min_x = xyz[0];
  float min_y = xyz[1];
  float min_z = xyz[2];
  float max_x = min_x;
  float max_y = min_y;
  float max_z = min_z;
  for (size_t i = 1; i < point_count; ++i) {
    const float x = xyz[i * 3];
    const float y = xyz[i * 3 + 1];
    const float z = xyz[i * 3 + 2];
    if (x < min_x) {
      min_x = x;
    }
    if (y < min_y) {
      min_y = y;
    }
    if (z < min_z) {
      min_z = z;
    }
    if (x > max_x) {
      max_x = x;
    }
    if (y > max_y) {
      max_y = y;
    }
    if (z > max_z) {
      max_z = z;
    }
  }
  min_xyz[0] = min_x;
  min_xyz[1] = min_y;
  min_xyz[2] = min_z;
  max_xyz[0] = max_x;
  max_xyz[1] = max_y;
  max_xyz[2] = max_z;
}

void find_xy_extrema(const double* x, const double* y, int n, int* i_n,
                     int* i_s, int* i_e, int* i_w) {
  if (n < 1) {
    *i_n = *i_s = *i_e = *i_w = 0;
    return;
  }
  // AVX2 extrema currently loses to scalar on short SoA (index gather cost);
  // keep the kernel for benches but dispatch stays scalar until rewritten.
  find_xy_extrema_scalar(x, y, n, i_n, i_s, i_e, i_w);
  (void)n;
}

int rdp_farthest_index(const double* x, const double* y, int lo, int hi,
                       double tol2) {
  if (hi - lo >= 16 && mesh_simd_avx2_runtime()) {
    return rdp_farthest_index_avx2(x, y, lo, hi, tol2);
  }
  return rdp_farthest_index_scalar(x, y, lo, hi, tol2);
}

void normalize_dirs_batch(Vec2* dirs, size_t count) {
  if (!dirs || count == 0) {
    return;
  }
  // Release matrix: AoS Vec2 normalize via vir-simd loses to scalar (~0.5x).
  // Keep avx2 kernel for benches; dispatch stays scalar.
  normalize_dirs_batch_scalar(dirs, count);
}

double path_length_xy(const double* x, const double* y, int n) {
  if (n < 2) {
    return 0;
  }
  if (n >= 16 && mesh_simd_avx2_runtime()) {
    return path_length_xy_avx2(x, y, n);
  }
  return path_length_xy_scalar(x, y, n);
}

void aabb_xyz_f32(const float* xyz, size_t point_count, float* min_xyz,
                  float* max_xyz) {
  if (!xyz || point_count == 0 || !min_xyz || !max_xyz) {
    return;
  }
  // AVX2 AABB path is AoS-unfriendly today; scalar wins. Kernel kept for
  // mesh_simd_benchmark matrix rows.
  aabb_xyz_f32_scalar(xyz, point_count, min_xyz, max_xyz);
  (void)point_count;
}

void append_mesh(TessMesh& dst, const TessMesh& src) {
  if (src.positions.empty() && src.indices.empty()) {
    return;
  }
  const uint32_t base = vert_count(dst);
  dst.positions.insert(dst.positions.end(), src.positions.begin(),
                       src.positions.end());
  dst.indices.reserve(dst.indices.size() + src.indices.size());
  for (uint32_t idx : src.indices) {
    dst.indices.push_back(base + idx);
  }
  dst.has_image = dst.has_image || src.has_image;
}

}  // namespace detail
}  // namespace vista

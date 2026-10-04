// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/process/nv/thrust_gis.h"

#include <cmath>
#include <cstdint>

#include <cuda_runtime.h>
#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>
#include <thrust/for_each.h>
#include <thrust/iterator/counting_iterator.h>

namespace vista {
namespace {

bool cuda_ready() {
  int n = 0;
  if (cudaGetDeviceCount(&n) != cudaSuccess || n < 1) {
    return false;
  }
  return true;
}

struct FillMaskOp {
  double minx;
  double miny;
  double maxx;
  double maxy;
  int cols;
  int rows;
  const double* ring_x;
  const double* ring_y;
  const int* ring_off;
  int ring_count;
  uint8_t* out;

  __host__ __device__ void operator()(int idx) const {
    const int col = idx % cols;
    const int row = idx / cols;
    const double dx =
        (maxx - minx) / static_cast<double>(cols > 1 ? cols - 1 : 1);
    const double dy =
        (maxy - miny) / static_cast<double>(rows > 1 ? rows - 1 : 1);
    const double lon = minx + static_cast<double>(col) * dx;
    const double lat = maxy - static_cast<double>(row) * dy;
    uint8_t hit = 0;
    for (int r = 0; r < ring_count; ++r) {
      const int a = ring_off[r];
      const int b = ring_off[r + 1];
      const int n = b - a;
      if (n < 3) {
        continue;
      }
      bool inside = false;
      int j = n - 1;
      for (int i = 0; i < n; ++i) {
        const double xi = ring_x[a + i];
        const double yi = ring_y[a + i];
        const double xj = ring_x[a + j];
        const double yj = ring_y[a + j];
        // Even-odd must match gis::detail::point_in_ring.
        const bool intersect =
            ((yi > lat) != (yj > lat)) &&
            (lon < (xj - xi) * (lat - yi) / ((yj - yi) + 0.0) + xi);
        if (intersect) {
          inside = !inside;
        }
        j = i;
      }
      if (inside) {
        hit = 1;
        break;
      }
    }
    out[idx] = hit;
  }
};

__host__ __device__ uint8_t pack_u8(float v) {
  if (v < 0.f) {
    v = 0.f;
  }
  if (v > 1.f) {
    v = 1.f;
  }
  return static_cast<uint8_t>(v * 255.f + 0.5f);
}

__host__ __device__ float sample_h(const float* h, int cols, int rows, int c,
                                   int r) {
  if (c < 0) {
    c = 0;
  }
  if (r < 0) {
    r = 0;
  }
  if (c >= cols) {
    c = cols - 1;
  }
  if (r >= rows) {
    r = rows - 1;
  }
  return h[r * cols + c];
}

struct ShadeOp {
  const float* heights;
  int cols;
  int rows;
  int step_x;
  int step_y;
  int w;
  int h;
  float dx_m;
  float dy_m;
  float exag;
  float az;
  float sin_alt;
  float cos_alt;
  float sr;
  float sg;
  float sb;
  float hr;
  float hg;
  float hb;
  uint8_t* rgba;

  __host__ __device__ void operator()(int idx) const {
    const int col = idx % w;
    const int row = idx / w;
    const int src_col = (col * step_x < cols) ? col * step_x : (cols - 1);
    const int src_row = (row * step_y < rows) ? row * step_y : (rows - 1);
    const float c = sample_h(heights, cols, rows, src_col, src_row);
    uint8_t* px = rgba + static_cast<size_t>(idx) * 4u;
    px[0] = 0;
    px[1] = 0;
    px[2] = 0;
    px[3] = 0;
    if (c <= 1.f) {
      return;
    }
    const float zw =
        sample_h(heights, cols, rows, src_col - step_x, src_row);
    const float ze =
        sample_h(heights, cols, rows, src_col + step_x, src_row);
    const float zs =
        sample_h(heights, cols, rows, src_col, src_row + step_y);
    const float zn =
        sample_h(heights, cols, rows, src_col, src_row - step_y);
    // Lambert term must match gis::detail::horn_lambert_shade.
    const float sx = -((ze - zw) / dx_m) * exag;
    const float sy = -((zn - zs) / dy_m) * exag;
    const float slope = atan(sqrt(sx * sx + sy * sy));
    float aspect = 0.f;
    if (sx != 0.f || sy != 0.f) {
      aspect = atan2(sy, -sx);
    }
    float shade = sin_alt * cos(slope) +
                  cos_alt * sin(slope) * cos(az - aspect);
    if (shade < 0.f) {
      shade = 0.f;
    }
    if (shade > 1.f) {
      shade = 1.f;
    }
    shade = (shade - 0.5f) * 1.80f + 0.5f;
    if (shade < 0.08f) {
      shade = 0.08f;
    }
    if (shade > 1.f) {
      shade = 1.f;
    }
    px[0] = pack_u8(sr + (hr - sr) * shade);
    px[1] = pack_u8(sg + (hg - sg) * shade);
    px[2] = pack_u8(sb + (hb - sb) * shade);
    px[3] = 255;
  }
};

}  // namespace

bool try_fill_lonlat_mask_thrust(double minx, double miny, double maxx,
                                 double maxy, int cols, int rows,
                                 const double* ring_x, const double* ring_y,
                                 const int* ring_off, int ring_count,
                                 int vert_count, uint8_t* out) {
  if (!cuda_ready() || !out || !ring_x || !ring_y || !ring_off || cols < 1 ||
      rows < 1 || ring_count < 1 || vert_count < 3) {
    return false;
  }
  const int n = cols * rows;
  try {
    thrust::device_vector<double> dx(ring_x, ring_x + vert_count);
    thrust::device_vector<double> dy(ring_y, ring_y + vert_count);
    thrust::device_vector<int> off(ring_off, ring_off + ring_count + 1);
    thrust::device_vector<uint8_t> dout(static_cast<size_t>(n), uint8_t{0});
    FillMaskOp op{minx,
                  miny,
                  maxx,
                  maxy,
                  cols,
                  rows,
                  thrust::raw_pointer_cast(dx.data()),
                  thrust::raw_pointer_cast(dy.data()),
                  thrust::raw_pointer_cast(off.data()),
                  ring_count,
                  thrust::raw_pointer_cast(dout.data())};
    thrust::for_each(thrust::device, thrust::counting_iterator<int>(0),
                     thrust::counting_iterator<int>(n), op);
    thrust::copy(dout.begin(), dout.end(), out);
  } catch (...) {
    return false;
  }
  return true;
}

bool try_shade_dem_thrust(const float* heights, int cols, int rows, int step_x,
                          int step_y, int w, int h, float dx_m, float dy_m,
                          float exag, float az, float sin_alt, float cos_alt,
                          float sr, float sg, float sb, float hr, float hg,
                          float hb, uint8_t* rgba) {
  if (!cuda_ready() || !heights || !rgba || cols < 2 || rows < 2 || w < 2 ||
      h < 2) {
    return false;
  }
  const int nsrc = cols * rows;
  const int npx = w * h;
  try {
    thrust::device_vector<float> dh(heights, heights + nsrc);
    thrust::device_vector<uint8_t> dr(static_cast<size_t>(npx) * 4u, uint8_t{0});
    ShadeOp op{thrust::raw_pointer_cast(dh.data()),
               cols,
               rows,
               step_x,
               step_y,
               w,
               h,
               dx_m,
               dy_m,
               exag,
               az,
               sin_alt,
               cos_alt,
               sr,
               sg,
               sb,
               hr,
               hg,
               hb,
               thrust::raw_pointer_cast(dr.data())};
    thrust::for_each(thrust::device, thrust::counting_iterator<int>(0),
                     thrust::counting_iterator<int>(npx), op);
    thrust::copy(dr.begin(), dr.end(), rgba);
  } catch (...) {
    return false;
  }
  return true;
}

bool thrust_gis_cuda_built() {
  return true;
}

}  // namespace vista

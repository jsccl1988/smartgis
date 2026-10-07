// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/nv/thrust_gis.h"

#include "vista/terrain/dem/nv/bake_pixel.h"

#include <cmath>
#include <cstdint>
#include <mutex>

#include <cuda_runtime.h>
#include <thrust/copy.h>
#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>
#include <thrust/for_each.h>
#include <thrust/iterator/counting_iterator.h>

namespace vista {
namespace {

std::mutex g_cuda_bake_mu;

bool cuda_ready() {
  int n = 0;
  if (cudaGetDeviceCount(&n) != cudaSuccess || n < 1) {
    return false;
  }
  return true;
}

struct ResidentDem {
  const float* hptr = nullptr;
  const uint8_t* lptr = nullptr;
  int cols = 0;
  int rows = 0;
  thrust::device_vector<float> dh;
  thrust::device_vector<uint8_t> dland;
};

ResidentDem& resident_dem() {
  static ResidentDem r;
  return r;
}

bool ensure_dem_heights(const float* heights, int cols, int rows) {
  if (!heights || cols < 1 || rows < 1) {
    return false;
  }
  const int n = cols * rows;
  ResidentDem& r = resident_dem();
  if (r.hptr == heights && r.cols == cols && r.rows == rows &&
      static_cast<int>(r.dh.size()) == n) {
    return true;
  }
  r.dh.assign(heights, heights + n);
  r.hptr = heights;
  r.cols = cols;
  r.rows = rows;
  r.lptr = nullptr;
  r.dland.clear();
  return true;
}

bool ensure_dem_land(const uint8_t* land, int cols, int rows) {
  ResidentDem& r = resident_dem();
  if (!land) {
    r.lptr = nullptr;
    r.dland.clear();
    return true;
  }
  const int n = cols * rows;
  if (r.lptr == land && r.cols == cols && r.rows == rows &&
      static_cast<int>(r.dland.size()) == n) {
    return true;
  }
  r.dland.assign(land, land + n);
  r.lptr = land;
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
    for (int ri = 0; ri < ring_count; ++ri) {
      const int a = ring_off[ri];
      const int b = ring_off[ri + 1];
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
    px[0] = detail::bake_pack_u8(sr + (hr - sr) * shade);
    px[1] = detail::bake_pack_u8(sg + (hg - sg) * shade);
    px[2] = detail::bake_pack_u8(sb + (hb - sb) * shade);
    px[3] = 255;
  }
};

struct HypsoOp {
  const float* heights;
  const uint8_t* land;
  int cols;
  int rows;
  int step_x;
  int step_y;
  int w;
  int h;
  uint8_t* rgba;

  __host__ __device__ void operator()(int idx) const {
    const int col = idx % w;
    const int row = idx / w;
    const int src_col = (col * step_x < cols) ? col * step_x : (cols - 1);
    const int src_row = (row * step_y < rows) ? row * step_y : (rows - 1);
    float r = 0.05f;
    float g = 0.08f;
    float b = 0.14f;
    bool is_land = true;
    if (land) {
      is_land = land[src_row * cols + src_col] != 0;
    }
    if (is_land) {
      const float m0 = sample_h(heights, cols, rows, src_col, src_row);
      float m = m0;
      if (m <= 1.f) {
        m = 80.f;
      }
      const int c1 = src_col + step_x < cols ? src_col + step_x : (cols - 1);
      const int r1 = src_row + step_y < rows ? src_row + step_y : (rows - 1);
      const float dh =
          fabsf(sample_h(heights, cols, rows, c1, src_row) - m0);
      const float dv =
          fabsf(sample_h(heights, cols, rows, src_col, r1) - m0);
      const float slope =
          detail::bake_clampf(sqrtf(dh * dh + dv * dv) / 900.f, 0.f, 1.f);
      detail::terrain_material_rgb_impl(m, slope, &r, &g, &b);
    }
    uint8_t* px = rgba + static_cast<size_t>(idx) * 4u;
    px[0] = detail::bake_pack_u8(r);
    px[1] = detail::bake_pack_u8(g);
    px[2] = detail::bake_pack_u8(b);
    px[3] = 255;
  }
};

__host__ __device__ bool hypso_land_at(const uint8_t* land, int cols, int rows,
                                       int step_x, int step_y, int c, int r) {
  if (!land) {
    return true;
  }
  const int src_col = (c * step_x < cols) ? c * step_x : (cols - 1);
  const int src_row = (r * step_y < rows) ? r * step_y : (rows - 1);
  return land[src_row * cols + src_col] != 0;
}

struct DilateOp {
  const uint8_t* src;
  const uint8_t* land;
  int cols;
  int rows;
  int step_x;
  int step_y;
  int w;
  int h;
  uint8_t* dst;

  __host__ __device__ void operator()(int idx) const {
    const int col = idx % w;
    const int row = idx / w;
    const size_t dst_i = static_cast<size_t>(idx) * 4u;
    dst[dst_i + 0] = src[dst_i + 0];
    dst[dst_i + 1] = src[dst_i + 1];
    dst[dst_i + 2] = src[dst_i + 2];
    dst[dst_i + 3] = src[dst_i + 3];
    if (hypso_land_at(land, cols, rows, step_x, step_y, col, row)) {
      return;
    }
    const int nbs[4][2] = {{col - 1, row},
                           {col + 1, row},
                           {col, row - 1},
                           {col, row + 1}};
    for (int k = 0; k < 4; ++k) {
      const int nc = nbs[k][0];
      const int nr = nbs[k][1];
      if (nc < 0 || nr < 0 || nc >= w || nr >= h) {
        continue;
      }
      const size_t src_i =
          (static_cast<size_t>(nr) * static_cast<size_t>(w) +
           static_cast<size_t>(nc)) *
          4u;
      const bool navy = src[src_i + 0] < 40 && src[src_i + 1] < 55 &&
                        src[src_i + 2] < 80 &&
                        !hypso_land_at(land, cols, rows, step_x, step_y, nc,
                                       nr);
      if (navy) {
        continue;
      }
      dst[dst_i + 0] = src[src_i + 0];
      dst[dst_i + 1] = src[src_i + 1];
      dst[dst_i + 2] = src[src_i + 2];
      dst[dst_i + 3] = 255;
      break;
    }
  }
};

struct JetFillOp {
  const float* heights;
  int cols;
  int rows;
  float zmin;
  float dz;
  uint8_t* rgba;

  __host__ __device__ void operator()(int idx) const {
    const float h = heights[idx];
    uint8_t* px = rgba + static_cast<size_t>(idx) * 4u;
    px[0] = 0;
    px[1] = 0;
    px[2] = 0;
    px[3] = 0;
    if (h < 1.f) {
      return;
    }
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    detail::jet_elevation_rgb_impl((h - zmin) / dz, &r, &g, &b);
    px[0] = detail::bake_pack_u8(r);
    px[1] = detail::bake_pack_u8(g);
    px[2] = detail::bake_pack_u8(b);
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
    std::lock_guard<std::mutex> lock(g_cuda_bake_mu);
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
  const int npx = w * h;
  try {
    std::lock_guard<std::mutex> lock(g_cuda_bake_mu);
    if (!ensure_dem_heights(heights, cols, rows)) {
      return false;
    }
    thrust::device_vector<uint8_t> dr(static_cast<size_t>(npx) * 4u, uint8_t{0});
    ShadeOp op{thrust::raw_pointer_cast(resident_dem().dh.data()),
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

bool try_bake_hypso_thrust(const float* heights, const uint8_t* land, int cols,
                           int rows, int step_x, int step_y, int w, int h,
                           uint8_t* rgba) {
  if (!cuda_ready() || !heights || !rgba || cols < 2 || rows < 2 || w < 2 ||
      h < 2) {
    return false;
  }
  const int npx = w * h;
  try {
    std::lock_guard<std::mutex> lock(g_cuda_bake_mu);
    if (!ensure_dem_heights(heights, cols, rows) ||
        !ensure_dem_land(land, cols, rows)) {
      return false;
    }
    thrust::device_vector<uint8_t> a(static_cast<size_t>(npx) * 4u, uint8_t{0});
    const uint8_t* dland = resident_dem().dland.empty()
                               ? nullptr
                               : thrust::raw_pointer_cast(resident_dem().dland.data());
    HypsoOp hop{thrust::raw_pointer_cast(resident_dem().dh.data()),
                dland,
                cols,
                rows,
                step_x,
                step_y,
                w,
                h,
                thrust::raw_pointer_cast(a.data())};
    thrust::for_each(thrust::device, thrust::counting_iterator<int>(0),
                     thrust::counting_iterator<int>(npx), hop);
    if (dland && w > 2 && h > 2) {
      thrust::device_vector<uint8_t> b(a.size());
      for (int pass = 0; pass < 3; ++pass) {
        uint8_t* srcp = (pass % 2 == 0)
                            ? thrust::raw_pointer_cast(a.data())
                            : thrust::raw_pointer_cast(b.data());
        uint8_t* dstp = (pass % 2 == 0)
                            ? thrust::raw_pointer_cast(b.data())
                            : thrust::raw_pointer_cast(a.data());
        DilateOp dop{srcp, dland, cols, rows, step_x, step_y, w, h, dstp};
        thrust::for_each(thrust::device, thrust::counting_iterator<int>(0),
                         thrust::counting_iterator<int>(npx), dop);
      }
      thrust::copy(b.begin(), b.end(), rgba);
    } else {
      thrust::copy(a.begin(), a.end(), rgba);
    }
  } catch (...) {
    return false;
  }
  return true;
}

bool try_jet_fill_thrust(const float* heights, int cols, int rows, float zmin,
                         float zmax, uint8_t* rgba) {
  if (!cuda_ready() || !heights || !rgba || cols < 2 || rows < 2) {
    return false;
  }
  if (!(zmax > zmin)) {
    zmin = 0.f;
    zmax = 1.f;
  }
  const int n = cols * rows;
  const float dz = zmax - zmin;
  try {
    std::lock_guard<std::mutex> lock(g_cuda_bake_mu);
    thrust::device_vector<float> dh(heights, heights + n);
    thrust::device_vector<uint8_t> dr(static_cast<size_t>(n) * 4u, uint8_t{0});
    JetFillOp op{thrust::raw_pointer_cast(dh.data()),
                 cols,
                 rows,
                 zmin,
                 dz,
                 thrust::raw_pointer_cast(dr.data())};
    thrust::for_each(thrust::device, thrust::counting_iterator<int>(0),
                     thrust::counting_iterator<int>(n), op);
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

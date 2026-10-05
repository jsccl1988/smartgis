// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_contour.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/terrain/dem/dem_frame.h"
#include "vista/terrain/process/bake_parallel.h"
#include "vista/terrain/process/nv/bake_pixel.h"
#include "vista/terrain/process/nv/thrust_gis.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vista {
namespace {

constexpr float kOceanMaxM = 1.f;

float clampf(float v, float lo, float hi) {
  return (std::max)(lo, (std::min)(hi, v));
}

void put_px(std::vector<uint8_t>* rgba, int cols, int rows, int x, int y,
            uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (!rgba || x < 0 || y < 0 || x >= cols || y >= rows) {
    return;
  }
  const std::size_t i =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
       static_cast<std::size_t>(x)) *
      4u;
  (*rgba)[i + 0] = r;
  (*rgba)[i + 1] = g;
  (*rgba)[i + 2] = b;
  (*rgba)[i + 3] = a;
}

void stamp_px(std::vector<uint8_t>* rgba, int cols, int rows, int x, int y,
              bool thick) {
  // White isolines (Origin stacked-surface look) on the jet sheet.
  put_px(rgba, cols, rows, x, y, 248, 252, 255, 255);
  if (!thick) {
    return;
  }
  put_px(rgba, cols, rows, x + 1, y, 248, 252, 255, 255);
  put_px(rgba, cols, rows, x, y + 1, 248, 252, 255, 255);
}

void stroke_seg(std::vector<uint8_t>* rgba, int cols, int rows, float x0,
                float y0, float x1, float y1, bool thick) {
  int ix0 = static_cast<int>(std::lround(x0));
  int iy0 = static_cast<int>(std::lround(y0));
  int ix1 = static_cast<int>(std::lround(x1));
  int iy1 = static_cast<int>(std::lround(y1));
  int dx = std::abs(ix1 - ix0);
  int dy = std::abs(iy1 - iy0);
  const int sx = ix0 < ix1 ? 1 : -1;
  const int sy = iy0 < iy1 ? 1 : -1;
  int err = dx - dy;
  for (;;) {
    stamp_px(rgba, cols, rows, ix0, iy0, thick);
    if (ix0 == ix1 && iy0 == iy1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      ix0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      iy0 += sy;
    }
  }
}

float edge_t(float ha, float hb, float z) {
  const float d = hb - ha;
  if (std::fabs(d) < 1e-6f) {
    return 0.5f;
  }
  return clampf((z - ha) / d, 0.f, 1.f);
}

void cell_segments(float h00, float h10, float h11, float h01, float z,
                   float c, float r, std::vector<float>* xy) {
  // Corners: 00 SW, 10 SE, 11 NE, 01 NW. Bit = above isolevel.
  const int mask = (h00 >= z ? 1 : 0) | (h10 >= z ? 2 : 0) |
                   (h11 >= z ? 4 : 0) | (h01 >= z ? 8 : 0);
  if (mask == 0 || mask == 15) {
    return;
  }
  const float t_s = edge_t(h00, h10, z);
  const float t_e = edge_t(h10, h11, z);
  const float t_n = edge_t(h01, h11, z);
  const float t_w = edge_t(h00, h01, z);
  const float sx = c + t_s;
  const float sy = r;
  const float ex = c + 1.f;
  const float ey = r + t_e;
  const float nx = c + t_n;
  const float ny = r + 1.f;
  const float wx = c;
  const float wy = r + t_w;
  auto emit = [&](float x0, float y0, float x1, float y1) {
    xy->push_back(x0);
    xy->push_back(y0);
    xy->push_back(x1);
    xy->push_back(y1);
  };
  switch (mask) {
    case 1:
    case 14:
      emit(sx, sy, wx, wy);
      break;
    case 2:
    case 13:
      emit(sx, sy, ex, ey);
      break;
    case 3:
    case 12:
      emit(wx, wy, ex, ey);
      break;
    case 4:
    case 11:
      emit(ex, ey, nx, ny);
      break;
    case 5:
      emit(sx, sy, wx, wy);
      emit(ex, ey, nx, ny);
      break;
    case 6:
    case 9:
      emit(sx, sy, nx, ny);
      break;
    case 7:
    case 8:
      emit(wx, wy, nx, ny);
      break;
    case 10:
      emit(sx, sy, ex, ey);
      emit(wx, wy, nx, ny);
      break;
    default:
      break;
  }
}

bool land_height_range(const float* heights, int cols, int rows,
                       float skip_below, float* zmin_out, float* zmax_out) {
  if (!heights || !zmin_out || !zmax_out || cols < 2 || rows < 2) {
    return false;
  }
  const std::size_t n =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  float zmin = 1.e9f;
  float zmax = -1.e9f;
  if (bake_rows_should_parallel(cols, rows)) {
    std::vector<float> row_min(static_cast<std::size_t>(rows), 1.e9f);
    std::vector<float> row_max(static_cast<std::size_t>(rows), -1.e9f);
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, rows, [&](int y) {
      float mn = 1.e9f;
      float mx = -1.e9f;
      const float* rowp = heights + static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(cols);
      for (int x = 0; x < cols; ++x) {
        const float hv = rowp[x];
        if (hv < skip_below) {
          continue;
        }
        mn = (std::min)(mn, hv);
        mx = (std::max)(mx, hv);
      }
      row_min[static_cast<std::size_t>(y)] = mn;
      row_max[static_cast<std::size_t>(y)] = mx;
    });
    for (int y = 0; y < rows; ++y) {
      zmin = (std::min)(zmin, row_min[static_cast<std::size_t>(y)]);
      zmax = (std::max)(zmax, row_max[static_cast<std::size_t>(y)]);
    }
  } else {
    for (std::size_t i = 0; i < n; ++i) {
      const float hv = heights[i];
      if (hv < skip_below) {
        continue;
      }
      zmin = (std::min)(zmin, hv);
      zmax = (std::max)(zmax, hv);
    }
  }
  if (!(zmax > zmin)) {
    return false;
  }
  *zmin_out = zmin;
  *zmax_out = zmax;
  return true;
}

void grid_lon_lat(int cols, int rows, double minx, double miny, double maxx,
                  double maxy, float col, float row, double* lon, double* lat) {
  const double dx = (maxx - minx) / static_cast<double>((std::max)(1, cols - 1));
  const double dy = (maxy - miny) / static_cast<double>((std::max)(1, rows - 1));
  // Row 0 = north / max-lat (matches bake_elevation_overlay_rgba).
  *lon = minx + static_cast<double>(col) * dx;
  *lat = maxy - static_cast<double>(row) * dy;
}

void emit_mesh_xyz(double lon, float elev, double lat, std::vector<float>* xyz) {
  xyz->push_back(dem_lon_to_x(lon));
  xyz->push_back(elev);
  xyz->push_back(static_cast<float>(lat));
}

}  // namespace

void jet_elevation_rgb(float t01, float* r, float* g, float* b) {
  detail::jet_elevation_rgb_impl(t01, r, g, b);
}

float pick_contour_interval_m(float zmin, float zmax) {
  const float span = zmax - zmin;
  if (!(span > 1.f)) {
    return 1.f;
  }
  const float raw = span / 12.f;
  const float mag = std::pow(10.f, std::floor(std::log10(raw)));
  const float n = raw / mag;
  float nice = 1.f;
  if (n >= 7.5f) {
    nice = 10.f;
  } else if (n >= 3.5f) {
    nice = 5.f;
  } else if (n >= 1.5f) {
    nice = 2.f;
  }
  return nice * mag;
}

bool bake_elevation_overlay_rgba(const float* heights, int cols, int rows,
                                 bool surface, bool curves, float interval_m,
                                 std::vector<uint8_t>* rgba) {
  if (!heights || !rgba || cols < 2 || rows < 2 || (!surface && !curves)) {
    return false;
  }
  const std::size_t n =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  float zmin = 0.f;
  float zmax = 1.f;
  if (!land_height_range(heights, cols, rows, kOceanMaxM, &zmin, &zmax)) {
    zmin = 0.f;
    zmax = 1.f;
  }
  const float dz = zmax - zmin;
  const float interval =
      interval_m > 0.f ? interval_m : pick_contour_interval_m(zmin, zmax);
  rgba->assign(n * 4u, 0);
  if (surface) {
    const BakeBackend backend = resolved_bake_backend();
    const bool allow_cuda = backend != BakeBackend::kCpu;
    const bool require_cuda = backend == BakeBackend::kCuda;
    bool filled = false;
    if (allow_cuda &&
        try_jet_fill_thrust(heights, cols, rows, zmin, zmax, rgba->data())) {
      filled = true;
    }
    if (!filled && require_cuda) {
      return false;
    }
    if (!filled) {
      uint8_t* px = rgba->data();
      auto fill_row = [&](int y) {
        for (int x = 0; x < cols; ++x) {
          const float hv =
              heights[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
                      static_cast<std::size_t>(x)];
          if (hv < kOceanMaxM) {
            continue;
          }
          float r = 0.f;
          float g = 0.f;
          float b = 0.f;
          jet_elevation_rgb((hv - zmin) / dz, &r, &g, &b);
          const std::size_t i =
              (static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
               static_cast<std::size_t>(x)) *
              4u;
          px[i + 0] = detail::bake_pack_u8(r);
          px[i + 1] = detail::bake_pack_u8(g);
          px[i + 2] = detail::bake_pack_u8(b);
          px[i + 3] = 255;
        }
      };
      if (bake_rows_should_parallel(cols, rows)) {
        base::execution::GlobalNThreadPoolExecutor executor;
        base::execution::parallel_for(executor, 0, rows, fill_row);
      } else {
        for (int y = 0; y < rows; ++y) {
          fill_row(y);
        }
      }
    }
  }
  if (!curves || interval <= 0.f) {
    return true;
  }
  float z0 = std::ceil(zmin / interval) * interval;
  if (z0 < zmin + 0.25f * interval) {
    z0 += interval;
  }
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>(cols) * 8u);
  int level_i = 0;
  for (float z = z0; z < zmax - 0.15f * interval; z += interval, ++level_i) {
    segs.clear();
    const bool thick = (level_i % 5) == 0;
    for (int y = 0; y < rows - 1; ++y) {
      for (int x = 0; x < cols - 1; ++x) {
        const std::size_t i00 =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
            static_cast<std::size_t>(x);
        const float h00 = heights[i00];
        const float h10 = heights[i00 + 1];
        const float h01 = heights[i00 + static_cast<std::size_t>(cols)];
        const float h11 = heights[i00 + static_cast<std::size_t>(cols) + 1];
        if (h00 < kOceanMaxM && h10 < kOceanMaxM && h01 < kOceanMaxM &&
            h11 < kOceanMaxM) {
          continue;
        }
        cell_segments(h00, h10, h11, h01, z, static_cast<float>(x),
                      static_cast<float>(y), &segs);
      }
    }
    for (std::size_t i = 0; i + 3 < segs.size(); i += 4) {
      stroke_seg(rgba, cols, rows, segs[i], segs[i + 1], segs[i + 2],
                 segs[i + 3], thick);
    }
  }
  return true;
}

bool extract_contour_curves_3d(const float* heights, int cols, int rows,
                               double minx, double miny, double maxx,
                               double maxy, float interval_m, float z_offset,
                               float skip_below,
                               std::vector<float>* xyz_segments) {
  if (!heights || !xyz_segments || cols < 2 || rows < 2 ||
      !(maxx > minx) || !(maxy > miny)) {
    return false;
  }
  float zmin = 0.f;
  float zmax = 1.f;
  if (!land_height_range(heights, cols, rows, skip_below, &zmin, &zmax)) {
    xyz_segments->clear();
    return false;
  }
  const float interval =
      interval_m > 0.f ? interval_m : pick_contour_interval_m(zmin, zmax);
  if (!(interval > 0.f)) {
    return false;
  }
  xyz_segments->clear();
  xyz_segments->reserve(static_cast<std::size_t>(cols) * 24u);
  float z0 = std::ceil(zmin / interval) * interval;
  if (z0 < zmin + 0.25f * interval) {
    z0 += interval;
  }
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>(cols) * 8u);
  for (float z = z0; z < zmax - 0.15f * interval; z += interval) {
    segs.clear();
    for (int y = 0; y < rows - 1; ++y) {
      for (int x = 0; x < cols - 1; ++x) {
        const std::size_t i00 =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
            static_cast<std::size_t>(x);
        const float h00 = heights[i00];
        const float h10 = heights[i00 + 1];
        const float h01 = heights[i00 + static_cast<std::size_t>(cols)];
        const float h11 = heights[i00 + static_cast<std::size_t>(cols) + 1];
        if (h00 < skip_below && h10 < skip_below && h01 < skip_below &&
            h11 < skip_below) {
          continue;
        }
        cell_segments(h00, h10, h11, h01, z, static_cast<float>(x),
                      static_cast<float>(y), &segs);
      }
    }
    // Isoline of height z: place vertices at elev = z + offset (true 3D
    // polylines in mesh space, not 2D texture strokes).
    const float elev = z + z_offset;
    for (std::size_t i = 0; i + 3 < segs.size(); i += 4) {
      double lon0 = 0.0;
      double lat0 = 0.0;
      double lon1 = 0.0;
      double lat1 = 0.0;
      grid_lon_lat(cols, rows, minx, miny, maxx, maxy, segs[i], segs[i + 1],
                   &lon0, &lat0);
      grid_lon_lat(cols, rows, minx, miny, maxx, maxy, segs[i + 2], segs[i + 3],
                   &lon1, &lat1);
      emit_mesh_xyz(lon0, elev, lat0, xyz_segments);
      emit_mesh_xyz(lon1, elev, lat1, xyz_segments);
    }
  }
  return !xyz_segments->empty();
}

bool build_contour_surface_tin(const float* heights, int cols, int rows,
                               double minx, double miny, double maxx,
                               double maxy, float z_offset, float skip_below,
                               float alpha, std::vector<float>* xyz,
                               std::vector<uint32_t>* indices,
                               std::vector<float>* rgba,
                               std::vector<float>* uvs) {
  if (!heights || !xyz || !indices || cols < 2 || rows < 2 || !(maxx > minx) ||
      !(maxy > miny)) {
    return false;
  }
  float zmin = 0.f;
  float zmax = 1.f;
  if (!land_height_range(heights, cols, rows, skip_below, &zmin, &zmax)) {
    xyz->clear();
    indices->clear();
    if (rgba) {
      rgba->clear();
    }
    if (uvs) {
      uvs->clear();
    }
    return false;
  }
  const float dz = zmax - zmin;
  const float a = clampf(alpha, 0.f, 1.f);
  const float u_den = static_cast<float>((std::max)(1, cols - 1));
  const float v_den = static_cast<float>((std::max)(1, rows - 1));
  const std::size_t n =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  std::vector<int> vert_of(n, -1);
  xyz->clear();
  indices->clear();
  if (rgba) {
    rgba->clear();
  }
  if (uvs) {
    uvs->clear();
  }
  xyz->reserve(n * 3u);
  if (rgba) {
    rgba->reserve(n * 4u);
  }
  if (uvs) {
    uvs->reserve(n * 2u);
  }
  int next = 0;
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      const std::size_t i =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
          static_cast<std::size_t>(x);
      const float hv = heights[i];
      if (hv < skip_below) {
        continue;
      }
      vert_of[i] = next++;
      double lon = 0.0;
      double lat = 0.0;
      grid_lon_lat(cols, rows, minx, miny, maxx, maxy, static_cast<float>(x),
                   static_cast<float>(y), &lon, &lat);
      emit_mesh_xyz(lon, hv + z_offset, lat, xyz);
      if (rgba) {
        float r = 0.f;
        float g = 0.f;
        float b = 0.f;
        jet_elevation_rgb((hv - zmin) / dz, &r, &g, &b);
        rgba->push_back(r);
        rgba->push_back(g);
        rgba->push_back(b);
        rgba->push_back(a);
      }
      if (uvs) {
        uvs->push_back(static_cast<float>(x) / u_den);
        // Row 0 = north → v=1 at top of drape texture.
        uvs->push_back(1.f - static_cast<float>(y) / v_den);
      }
    }
  }
  if (next < 3) {
    xyz->clear();
    if (rgba) {
      rgba->clear();
    }
    if (uvs) {
      uvs->clear();
    }
    return false;
  }
  indices->reserve(static_cast<std::size_t>((cols - 1) * (rows - 1) * 6));
  for (int y = 0; y < rows - 1; ++y) {
    for (int x = 0; x < cols - 1; ++x) {
      const std::size_t i00 =
          static_cast<std::size_t>(y) * static_cast<std::size_t>(cols) +
          static_cast<std::size_t>(x);
      const std::size_t i10 = i00 + 1;
      const std::size_t i01 = i00 + static_cast<std::size_t>(cols);
      const std::size_t i11 = i01 + 1;
      const int v00 = vert_of[i00];
      const int v10 = vert_of[i10];
      const int v01 = vert_of[i01];
      const int v11 = vert_of[i11];
      if (v00 < 0 || v10 < 0 || v01 < 0 || v11 < 0) {
        continue;
      }
      // Match DemRaster::build_mesh winding.
      indices->push_back(static_cast<uint32_t>(v00));
      indices->push_back(static_cast<uint32_t>(v11));
      indices->push_back(static_cast<uint32_t>(v10));
      indices->push_back(static_cast<uint32_t>(v00));
      indices->push_back(static_cast<uint32_t>(v01));
      indices->push_back(static_cast<uint32_t>(v11));
    }
  }
  return !indices->empty();
}

}  // namespace vista

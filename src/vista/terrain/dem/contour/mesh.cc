// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/contour/mesh.h"

#include "vista/terrain/dem/contour/levels.h"
#include "vista/terrain/dem/dem_contour.h"
#include "vista/terrain/dem/dem_frame.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vista {
namespace detail {
namespace {

float clampf(float v, float lo, float hi) {
  return (std::max)(lo, (std::min)(hi, v));
}

}  // namespace

void grid_lon_lat(int cols, int rows, double minx, double miny, double maxx,
                  double maxy, float col, float row, double* lon, double* lat) {
  const double dx =
      (maxx - minx) / static_cast<double>((std::max)(1, cols - 1));
  const double dy =
      (maxy - miny) / static_cast<double>((std::max)(1, rows - 1));
  // Row 0 = north / max-lat (matches bake_elevation_overlay_rgba).
  *lon = minx + static_cast<double>(col) * dx;
  *lat = maxy - static_cast<double>(row) * dy;
}

void emit_mesh_xyz(double lon, float elev, double lat,
                   std::vector<float>* xyz) {
  xyz->push_back(dem_lon_to_x(lon));
  xyz->push_back(elev);
  xyz->push_back(static_cast<float>(lat));
}

void emit_isoline_mesh_xyz(int cols, int rows, double minx, double miny,
                           double maxx, double maxy, float elev,
                           const std::vector<float>& segs,
                           std::vector<float>* xyz_segments) {
  if (!xyz_segments) {
    return;
  }
  for (std::size_t i = 0; i + 3u < segs.size(); i += 4u) {
    double lon0 = 0.0;
    double lat0 = 0.0;
    double lon1 = 0.0;
    double lat1 = 0.0;
    grid_lon_lat(cols, rows, minx, miny, maxx, maxy, segs[i], segs[i + 1u],
                 &lon0, &lat0);
    grid_lon_lat(cols, rows, minx, miny, maxx, maxy, segs[i + 2u],
                 segs[i + 3u], &lon1, &lat1);
    emit_mesh_xyz(lon0, elev, lat0, xyz_segments);
    emit_mesh_xyz(lon1, elev, lat1, xyz_segments);
  }
}

bool build_surface_tin(const float* heights, int cols, int rows, double minx,
                       double miny, double maxx, double maxy, float z_offset,
                       float skip_below, float alpha, std::vector<float>* xyz,
                       std::vector<uint32_t>* indices, std::vector<float>* rgba,
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

}  // namespace detail
}  // namespace vista

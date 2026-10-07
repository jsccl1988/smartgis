// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_contour.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/terrain/dem/contour/levels.h"
#include "vista/terrain/dem/contour/mesh.h"
#include "vista/terrain/dem/contour/paint.h"
#include "vista/terrain/dem/contour/smooth.h"
#include "vista/terrain/dem/bake/bake_backend.h"
#include "vista/terrain/dem/bake/bake_parallel.h"
#include "vista/terrain/dem/nv/bake_pixel.h"
#include "vista/terrain/dem/nv/thrust_gis.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vista {

void jet_elevation_rgb(float t01, float* r, float* g, float* b) {
  detail::jet_elevation_rgb_impl(t01, r, g, b);
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
  if (!detail::land_height_range(heights, cols, rows, detail::kDemOceanMaxM,
                                 &zmin, &zmax)) {
    zmin = 0.f;
    zmax = 1.f;
  }
  const float dz = zmax - zmin;
  float interval = 0.f;
  float z0 = 0.f;
  const bool have_levels =
      detail::contour_level_bounds(zmin, zmax, interval_m, &interval, &z0);
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
              heights[static_cast<std::size_t>(y) *
                          static_cast<std::size_t>(cols) +
                      static_cast<std::size_t>(x)];
          if (hv < detail::kDemOceanMaxM) {
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
  if (!curves || !have_levels) {
    return true;
  }
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>(cols) * 8u);
  int level_i = 0;
  for (float z = z0; z < zmax - 0.15f * interval; z += interval, ++level_i) {
    detail::collect_isoline_xy(heights, cols, rows, z, detail::kDemOceanMaxM,
                               detail::kContourChaikinIters, &segs);
    detail::stroke_isoline_xy(rgba, cols, rows, segs, (level_i % 5) == 0);
  }
  return true;
}

bool extract_contour_curves_3d(const float* heights, int cols, int rows,
                               double minx, double miny, double maxx,
                               double maxy, float interval_m, float z_offset,
                               float skip_below,
                               std::vector<float>* xyz_segments) {
  if (!heights || !xyz_segments || cols < 2 || rows < 2 || !(maxx > minx) ||
      !(maxy > miny)) {
    return false;
  }
  float zmin = 0.f;
  float zmax = 1.f;
  if (!detail::land_height_range(heights, cols, rows, skip_below, &zmin,
                                 &zmax)) {
    xyz_segments->clear();
    return false;
  }
  float interval = 0.f;
  float z0 = 0.f;
  if (!detail::contour_level_bounds(zmin, zmax, interval_m, &interval, &z0)) {
    return false;
  }
  xyz_segments->clear();
  xyz_segments->reserve(static_cast<std::size_t>(cols) * 24u);
  std::vector<float> segs;
  segs.reserve(static_cast<std::size_t>(cols) * 8u);
  for (float z = z0; z < zmax - 0.15f * interval; z += interval) {
    detail::collect_isoline_xy(heights, cols, rows, z, skip_below,
                               detail::kContourChaikinIters, &segs);
    // Isoline of height z: place vertices at elev = z + offset (true 3D
    // polylines in mesh space, not 2D texture strokes).
    detail::emit_isoline_mesh_xyz(cols, rows, minx, miny, maxx, maxy,
                                  z + z_offset, segs, xyz_segments);
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
  return detail::build_surface_tin(heights, cols, rows, minx, miny, maxx, maxy,
                                   z_offset, skip_below, alpha, xyz, indices,
                                   rgba, uvs);
}

}  // namespace vista

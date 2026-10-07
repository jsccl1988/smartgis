// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#include "vista/terrain/dem/dem_frame.h"
#include "vista/terrain/dem/raster/bake_util.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vista {

bool DemRaster::build_mesh(int max_edge, std::vector<float>* xyz,
                           std::vector<uint32_t>* indices) const {
  return build_mesh(max_edge, xyz, indices, nullptr);
}

bool DemRaster::build_mesh(int max_edge, std::vector<float>* xyz,
                           std::vector<uint32_t>* indices,
                           std::vector<float>* uvs) const {
  if (empty()) {
    return false;
  }
  return build_mesh_window(minx_, miny_, maxx_, maxy_, max_edge, xyz, indices,
                           uvs);
}

bool DemRaster::build_mesh_window(double minx, double miny, double maxx,
                                  double maxy, int max_edge,
                                  std::vector<float>* xyz,
                                  std::vector<uint32_t>* indices,
                                  std::vector<float>* uvs,
                                  bool apply_land_mask) const {
  if (!xyz || !indices || empty()) {
    return false;
  }
  // Intersect with DEM envelope.
  const double win_minx = (std::max)(minx, minx_);
  const double win_miny = (std::max)(miny, miny_);
  const double win_maxx = (std::min)(maxx, maxx_);
  const double win_maxy = (std::min)(maxy, maxy_);
  if (!(win_maxx > win_minx) || !(win_maxy > win_miny)) {
    return false;
  }
  const double dx = (maxx_ - minx_) / (std::max)(1, cols_ - 1);
  const double dy = (maxy_ - miny_) / (std::max)(1, rows_ - 1);
  int col0 = static_cast<int>(std::floor((win_minx - minx_) / dx));
  int col1 = static_cast<int>(std::ceil((win_maxx - minx_) / dx));
  int row0 = static_cast<int>(std::floor((maxy_ - win_maxy) / dy));
  int row1 = static_cast<int>(std::ceil((maxy_ - win_miny) / dy));
  col0 = (std::max)(0, (std::min)(cols_ - 1, col0));
  col1 = (std::max)(0, (std::min)(cols_ - 1, col1));
  row0 = (std::max)(0, (std::min)(rows_ - 1, row0));
  row1 = (std::max)(0, (std::min)(rows_ - 1, row1));
  if (col1 <= col0 || row1 <= row0) {
    return false;
  }
  const int win_cols = col1 - col0 + 1;
  const int win_rows = row1 - row0 + 1;
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (win_cols > max_edge) {
      step_x = win_cols / max_edge;
    }
    if (win_rows > max_edge) {
      step_y = win_rows / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int mc = (win_cols + step_x - 1) / step_x;
  const int mr = (win_rows + step_y - 1) / step_y;
  if (mc < 2 || mr < 2) {
    return false;
  }
  xyz->clear();
  indices->clear();
  if (uvs) {
    uvs->clear();
  }
  auto is_land = [&](int src_col, int src_row) -> bool {
    if (!apply_land_mask || land_.empty()) {
      return true;
    }
    src_col = (std::max)(0, (std::min)(cols_ - 1, src_col));
    src_row = (std::max)(0, (std::min)(rows_ - 1, src_row));
    return land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
  };
  std::vector<int> vert_of(static_cast<size_t>(mc * mr), -1);
  // UVs must address the full DEM hypsometric/drape texture (bake covers
  // cols_×rows_), not the local mesh window grid. Window-local 0..1 made
  // land-only meshes sample ocean texels → black FlyCube DEM slabs.
  const float u_den = static_cast<float>((std::max)(1, cols_ - 1));
  const float v_den = static_cast<float>((std::max)(1, rows_ - 1));
  std::vector<uint8_t> keep(static_cast<size_t>(mc * mr), 0);
  std::vector<int> row_n(static_cast<size_t>(mr), 0);
  auto mark_row = [&](int row) {
    const int src_row =
        (std::min)(row1, row0 + (std::min)(win_rows - 1, row * step_y));
    int n = 0;
    for (int col = 0; col < mc; ++col) {
      const int src_col =
          (std::min)(col1, col0 + (std::min)(win_cols - 1, col * step_x));
      if (!is_land(src_col, src_row)) {
        continue;
      }
      keep[static_cast<size_t>(row * mc + col)] = 1;
      ++n;
    }
    row_n[static_cast<size_t>(row)] = n;
  };
  detail::for_each_bake_row(mc, mr, mark_row);
  std::vector<int> row_off(static_cast<size_t>(mr) + 1u, 0);
  for (int row = 0; row < mr; ++row) {
    row_off[static_cast<size_t>(row) + 1] =
        row_off[static_cast<size_t>(row)] + row_n[static_cast<size_t>(row)];
  }
  const int nverts = row_off[static_cast<size_t>(mr)];
  if (nverts < 3) {
    return false;
  }
  xyz->assign(static_cast<size_t>(nverts) * 3u, 0.f);
  if (uvs) {
    uvs->assign(static_cast<size_t>(nverts) * 2u, 0.f);
  }
  float* xyzp = xyz->data();
  float* uvp = uvs ? uvs->data() : nullptr;
  auto fill_row = [&](int row) {
    const int src_row =
        (std::min)(row1, row0 + (std::min)(win_rows - 1, row * step_y));
    const double lat = maxy_ - src_row * dy;
    int out = row_off[static_cast<size_t>(row)];
    for (int col = 0; col < mc; ++col) {
      if (!keep[static_cast<size_t>(row * mc + col)]) {
        continue;
      }
      const int src_col =
          (std::min)(col1, col0 + (std::min)(win_cols - 1, col * step_x));
      vert_of[static_cast<size_t>(row * mc + col)] = out;
      const double lon = minx_ + src_col * dx;
      const float ht = meters_at(src_col, src_row) * vert_exag_;
      const size_t o = static_cast<size_t>(out) * 3u;
      xyzp[o + 0] = dem_lon_to_x(lon);
      xyzp[o + 1] = ht;
      xyzp[o + 2] = static_cast<float>(lat);
      if (uvp) {
        uvp[static_cast<size_t>(out) * 2u + 0] =
            static_cast<float>(src_col) / u_den;
        uvp[static_cast<size_t>(out) * 2u + 1] =
            static_cast<float>(src_row) / v_den;
      }
      ++out;
    }
  };
  detail::for_each_bake_row(mc, mr, fill_row);
  indices->reserve(static_cast<size_t>((mc - 1) * (mr - 1) * 6));
  for (int row = 0; row < mr - 1; ++row) {
    for (int col = 0; col < mc - 1; ++col) {
      const int i00 = vert_of[static_cast<size_t>(row * mc + col)];
      const int i10 = vert_of[static_cast<size_t>(row * mc + col + 1)];
      const int i01 = vert_of[static_cast<size_t>((row + 1) * mc + col)];
      const int i11 = vert_of[static_cast<size_t>((row + 1) * mc + col + 1)];
      if (i00 < 0 || i10 < 0 || i01 < 0 || i11 < 0) {
        continue;
      }
      indices->push_back(static_cast<uint32_t>(i00));
      indices->push_back(static_cast<uint32_t>(i11));
      indices->push_back(static_cast<uint32_t>(i10));
      indices->push_back(static_cast<uint32_t>(i00));
      indices->push_back(static_cast<uint32_t>(i01));
      indices->push_back(static_cast<uint32_t>(i11));
    }
  }
  return !indices->empty();
}

}  // namespace vista

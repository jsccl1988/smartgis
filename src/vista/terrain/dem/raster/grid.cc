// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#include "vista/terrain/dem/detail/dem_simd.h"
#include "vista/terrain/dem/mask/land_mask.h"
#include "vista/terrain/dem/raster/bake_util.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace vista {

void DemRaster::fit_vertical_exaggeration() {
  const float span =
      static_cast<float>((std::max)(maxx_ - minx_, maxy_ - miny_));
  const float peak = (std::max)(80.f, max_m_ - min_m_);
  // Match leftover DemHeightField (span * 0.09 / peak): milder relief so
  // draped imagery and orbit pitch read like SmartGis.exe.
  vert_exag_ = (span * 0.09f) / peak;
}

float DemRaster::meters_at(int col, int row) const {
  col = (std::max)(0, (std::min)(cols_ - 1, col));
  row = (std::max)(0, (std::min)(rows_ - 1, row));
  return heights_[static_cast<size_t>(index_at(col, row))];
}

void DemRaster::recompute_range() {
  if (heights_.empty()) {
    min_m_ = 0;
    max_m_ = 1;
    return;
  }
  detail::minmax_f32(heights_.data(), heights_.size(), &min_m_, &max_m_);
  if (max_m_ <= min_m_) {
    max_m_ = min_m_ + 1.f;
  }
}

void DemRaster::downsample_to_max_edge(int max_edge) {
  if (max_edge < 8 || empty()) {
    return;
  }
  const int long_edge = (std::max)(cols_, rows_);
  if (long_edge <= max_edge) {
    return;
  }
  const int new_cols = (std::max)(2, cols_ * max_edge / long_edge);
  const int new_rows = (std::max)(2, rows_ * max_edge / long_edge);
  std::vector<float> next(static_cast<size_t>(new_cols * new_rows));
  const float* src = heights_.data();
  const int src_cols = cols_;
  const int src_rows = rows_;
  auto fill_row = [&](int row) {
    const int src_row = row * src_rows / new_rows;
    float* dst =
        next.data() + static_cast<size_t>(row) * static_cast<size_t>(new_cols);
    for (int col = 0; col < new_cols; ++col) {
      const int src_col = col * src_cols / new_cols;
      dst[col] = src[static_cast<size_t>(src_row) * static_cast<size_t>(src_cols) +
                     static_cast<size_t>(src_col)];
    }
  };
  detail::for_each_bake_row(new_cols, new_rows, fill_row);
  heights_.swap(next);
  cols_ = new_cols;
  rows_ = new_rows;
  // Rebuild land_ after resize. Clearing without rebuild left land_ empty so
  // bake treated every cell as land while mesh windows that later call
  // mask_outside_rings stayed coherent — but a post-downsample empty mask
  // also dropped the height>1 land cue used by hypsometric ocean navy.
  land_.assign(heights_.size(), 0);
  for (size_t i = 0; i < heights_.size(); ++i) {
    land_[i] = heights_[i] > 1.f ? 1 : 0;
  }
}

void DemRaster::mask_outside_rings(const std::vector<LonLatRing>& rings) {
  if (empty() || rings.empty()) {
    return;
  }
  land_.assign(static_cast<size_t>(cols_ * rows_), 0);
  fill_lonlat_mask(minx_, miny_, maxx_, maxy_, cols_, rows_, rings,
                   land_.data());
  for (size_t i = 0; i < heights_.size(); ++i) {
    if (!land_[i]) {
      heights_[i] = 0.f;
    }
  }
  recompute_range();
  fit_vertical_exaggeration();
}

float DemRaster::sample_meters(double x, double y) const {
  if (empty()) {
    return 0.f;
  }
  const double dx = maxx_ - minx_;
  const double dy = maxy_ - miny_;
  if (dx <= 0.0 || dy <= 0.0) {
    return 0.f;
  }
  const double u = (x - minx_) / dx * (cols_ - 1);
  const double v = (maxy_ - y) / dy * (rows_ - 1);
  const int c0 = static_cast<int>(std::floor(u));
  const int r0 = static_cast<int>(std::floor(v));
  const float tx = static_cast<float>(u - c0);
  const float ty = static_cast<float>(v - r0);
  const float h00 = meters_at(c0, r0);
  const float h10 = meters_at(c0 + 1, r0);
  const float h01 = meters_at(c0, r0 + 1);
  const float h11 = meters_at(c0 + 1, r0 + 1);
  const float h0 = h00 * (1.f - tx) + h10 * tx;
  const float h1 = h01 * (1.f - tx) + h11 * tx;
  return h0 * (1.f - ty) + h1 * ty;
}

float DemRaster::sample(double x, double y) const {
  return sample_meters(x, y) * vert_exag_;
}

void DemRaster::envelope(double* minx, double* miny, double* maxx,
                         double* maxy) const {
  if (minx) {
    *minx = minx_;
  }
  if (miny) {
    *miny = miny_;
  }
  if (maxx) {
    *maxx = maxx_;
  }
  if (maxy) {
    *maxy = maxy_;
  }
}

int DemRaster::lod_max_edge(float camera_distance, int min_edge,
                            int max_edge_cap) {
  if (min_edge < 8) {
    min_edge = 8;
  }
  if (max_edge_cap < min_edge) {
    max_edge_cap = min_edge;
  }
  // Orbit framing: distance ~2.55 is the full/land showcase — keep the dense
  // bucket (max_edge_cap) so coasts are not stair-stepped. Coarser LODs only
  // kick in past the overview (~3.0+).
  int edge = max_edge_cap;
  if (camera_distance > 8.0f) {
    edge = min_edge;
  } else if (camera_distance > 5.0f) {
    edge = min_edge + (max_edge_cap - min_edge) / 4;
  } else if (camera_distance > 3.6f) {
    edge = min_edge + (max_edge_cap - min_edge) / 2;
  } else if (camera_distance > 3.0f) {
    edge = min_edge + 3 * (max_edge_cap - min_edge) / 4;
  }
  if (edge < min_edge) {
    edge = min_edge;
  }
  if (edge > max_edge_cap) {
    edge = max_edge_cap;
  }
  return edge;
}

int DemRaster::lod_expected_vertices(int cols, int rows, int max_edge) {
  if (cols < 2 || rows < 2) {
    return 0;
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols > max_edge) {
      step_x = cols / max_edge;
    }
    if (rows > max_edge) {
      step_y = rows / max_edge;
    }
  }
  if (step_x < 1) {
    step_x = 1;
  }
  if (step_y < 1) {
    step_y = 1;
  }
  const int mc = (cols + step_x - 1) / step_x;
  const int mr = (rows + step_y - 1) / step_y;
  return mc * mr;
}

}  // namespace vista

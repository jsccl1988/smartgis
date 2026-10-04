// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RENDER_RHI2D_IMPL_COMMON_PAINT_CARTO_DRAW_LATTICE_VIEW_H_
#define SCENIC_RENDER_RHI2D_IMPL_COMMON_PAINT_CARTO_DRAW_LATTICE_VIEW_H_

namespace scenic {
namespace detail {

// Non-owning structured XY lattice for carto mesh draw. Ownership stays
// above this layer (plugin / content / gis); bind nx/ny + sample_xy.
// Index: col in [0, nx), row in [0, ny) — same as geo::NodeField2d.
struct LatticeView2d {
  int nx = 0;
  int ny = 0;
  bool (*sample_xy)(const void* ctx, int col, int row, double* x,
                    double* y) = nullptr;
  const void* ctx = nullptr;

  bool is_empty() const { return nx < 1 || ny < 1 || sample_xy == nullptr; }

  bool point(int col, int row, double* x, double* y) const {
    if (sample_xy == nullptr) {
      return false;
    }
    return sample_xy(ctx, col, row, x, y);
  }
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RENDER_RHI2D_IMPL_COMMON_PAINT_CARTO_DRAW_LATTICE_VIEW_H_

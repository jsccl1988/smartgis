// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEXGRID_SAMPLE_VOLUME_H_
#define PLUGIN_WORLD3D_HEXGRID_SAMPLE_VOLUME_H_

#include "plugin/product/world3d/scene/hexgrid/solve/boundary_solve.h"

namespace plugin {
namespace detail {

// Menu / processing defaults: many hex cells over a GIS-scale quarry, not a
// unit cube or 2x2 lattice.
inline constexpr int k_demo_nx = 24;
inline constexpr int k_demo_ny = 20;
inline constexpr int k_demo_nz = 12;

// Seeded LCG used only for the in-memory quarry TIN (deterministic).
inline constexpr unsigned k_demo_rng_seed = 0xC0FFEEu;

// Build a hex volume from two Delaunay TINs (ground surface + pit floor)
// over an irregular projected-meter footprint.
HexCornerSolve solve_hex_from_quarry_sample(int nx, int ny, int nz);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_HEXGRID_SAMPLE_VOLUME_H_

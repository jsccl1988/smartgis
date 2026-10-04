// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEXGRID_BOUNDARY_SOLVE_H_
#define PLUGIN_WORLD3D_HEXGRID_BOUNDARY_SOLVE_H_

#include <string>
#include <vector>

#include "plugin/product/world3d/grid/hexgrid/lattice/hex_lattice.h"

namespace plugin {
namespace detail {

// Eight corners of the logical unit cube in ijk order:
// (0,0,0) (1,0,0) (1,1,0) (0,1,0) (0,0,1) (1,0,1) (1,1,1) (0,1,1).
struct HexCornerSolve {
  bool ok = false;
  HexLattice grid;
  std::vector<float> cell_orth;
  std::string message;
};

// Fill XYZ lattice from 8 corners (trilinear seed + face Dirichlet + Laplace).
HexCornerSolve solve_hex_from_corners(const Xyz corners[8],
                                      int nx,
                                      int ny,
                                      int nz);

// Laplace-smooth a fully seeded (i,j,k) lattice. Face nodes stay Dirichlet.
HexCornerSolve solve_hex_from_nodes(int nx,
                                    int ny,
                                    int nz,
                                    std::vector<double> xs,
                                    std::vector<double> ys,
                                    std::vector<double> zs);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_HEXGRID_BOUNDARY_SOLVE_H_

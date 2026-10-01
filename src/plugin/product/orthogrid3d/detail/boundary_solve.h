// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID3D_BOUNDARY_SOLVE_H_
#define PLUGIN_ORTHOGRID3D_BOUNDARY_SOLVE_H_

#include <string>
#include <vector>

#include "gis/kernel/geo/mesh/geometry.h"

namespace plugin {
namespace detail {

// Eight corners of the logical unit cube in ijk order:
// (0,0,0) (1,0,0) (1,1,0) (0,1,0) (0,0,1) (1,0,1) (1,1,1) (0,1,1).
struct HexCornerSolve {
  bool ok = false;
  geo::HexGrid grid;
  std::vector<float> cell_orth;
  std::string message;
};

// Fill HexGrid from 8 corners (trilinear seed + face Dirichlet + Laplace).
HexCornerSolve solve_hex_from_corners(const geo::Raw3DPoint corners[8],
                                      int nx,
                                      int ny,
                                      int nz);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID3D_BOUNDARY_SOLVE_H_

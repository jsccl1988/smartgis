// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_LAPLACE_SOLVER_H_
#define PLUGIN_ORTHOGRID_LAPLACE_SOLVER_H_

#include <cstdint>

namespace orthogrid {

// Structured 2D node field in row-major order: index = j * nx + i.
struct GridField {
  int nx = 0;
  int ny = 0;
  double* x = nullptr;
  double* y = nullptr;
};

// Discrete 5-point Laplace (Dirichlet where is_unknown[j*nx+i] == 0).
// Assembles a sparse Ax=b and solves with Eigen SparseLU. Unknown nodes
// are overwritten; Dirichlet nodes keep their incoming values.
bool solve_laplace(const GridField& grid, const std::uint8_t* is_unknown);

// One frozen-coefficient Thompson elliptic step (α, β, γ from the current
// grid; control functions P = Q = 0). Same unknown mask as solve_laplace.
bool solve_elliptic_step(const GridField& grid, const std::uint8_t* is_unknown);

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_LAPLACE_SOLVER_H_

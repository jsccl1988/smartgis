// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID3D_LAPLACE_SOLVER_H_
#define PLUGIN_ORTHOGRID3D_LAPLACE_SOLVER_H_

#include <cstdint>

namespace orthogrid3d {

// Structured 3D node field: index = k * ny * nx + j * nx + i.
struct VolumeField {
  int nx = 0;
  int ny = 0;
  int nz = 0;
  double* x = nullptr;
  double* y = nullptr;
  double* z = nullptr;
};

// Discrete 7-point Laplace (Dirichlet where is_unknown == 0).
// Assembles sparse Ax=b and solves with Eigen SparseLU for x,y,z.
bool solve_laplace_3d(const VolumeField& field, const std::uint8_t* is_unknown);

}  // namespace orthogrid3d

#endif  // PLUGIN_ORTHOGRID3D_LAPLACE_SOLVER_H_

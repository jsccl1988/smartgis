// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_LAPLACE_SOLVER_H_
#define PLUGIN_ORTHOGRID_LAPLACE_SOLVER_H_

#include <cstdint>
#include <vector>

#include <Eigen/Core>

namespace orthogrid {

// Row-major (ny × nx) node coordinates; access is (j, i). Flat export uses
// index = j * nx + i (same layout as legacy BoundarySolve vectors).
using GridArray =
    Eigen::Array<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

// Structured 2D node field owned as Eigen arrays.
struct GridField {
  int nx = 0;
  int ny = 0;
  GridArray x;
  GridArray y;

  // Resize and copy from flat row-major buffers (size nx*ny).
  void assign_from_flat(int nx_in,
                        int ny_in,
                        const double* xs,
                        const double* ys);

  // Write row-major flat buffers (resized to nx*ny).
  void copy_to_flat(std::vector<double>* xs, std::vector<double>* ys) const;

  bool is_shaped() const {
    return nx >= 3 && ny >= 3 && x.rows() == ny && x.cols() == nx &&
           y.rows() == ny && y.cols() == nx;
  }
};

// Discrete 5-point Laplace (Dirichlet where is_unknown[j*nx+i] == 0).
// Assembles sparse Ax=b; SimplicialLDLT; one factorize for x and y RHS.
bool solve_laplace(GridField& grid, const std::uint8_t* is_unknown);

// One frozen-coefficient Thompson elliptic step (α, β, γ from the current
// grid; control functions P = Q = 0). Same unknown mask as solve_laplace.
bool solve_elliptic_step(GridField& grid, const std::uint8_t* is_unknown);

// N Thompson steps with a single analyzePattern (sparsity fixed by mask).
bool solve_elliptic_steps(GridField& grid,
                          const std::uint8_t* is_unknown,
                          int iters);

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_LAPLACE_SOLVER_H_

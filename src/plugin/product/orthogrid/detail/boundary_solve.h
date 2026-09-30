// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_
#define PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_

#include <string>
#include <vector>

namespace plugin {
namespace detail {

// Product-side gridbnd load + Dirichlet Laplace. Does not link the 2010
// Orthogrid class (that class pulls the legacy GIS/MFC graph).
struct BoundarySolve {
  bool ok = false;
  int nx = 0;
  int ny = 0;
  int node_count = 0;
  // Solved node coordinates (row-major: index = j * nx + i). Empty on failure.
  std::vector<double> xs;
  std::vector<double> ys;
  // JSON the processing host already forwards: {"ok":true,"nodes":N}
  // or {"error":"..."}.
  std::string message;
};

BoundarySolve solve_grid_boundary_file(const std::string& path);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_

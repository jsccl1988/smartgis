// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_
#define PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_

#include <string>

namespace plugin {
namespace detail {

// Product-side gridbnd load + Dirichlet Laplace. Does not link the 2010
// Orthogrid class (that class pulls the legacy GIS/MFC graph).
struct BoundarySolve {
  bool ok = false;
  int node_count = 0;
  // JSON the processing host already forwards: {"ok":true,"nodes":N}
  // or {"error":"..."}.
  std::string message;
};

BoundarySolve solve_grid_boundary_file(const std::string& path);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_

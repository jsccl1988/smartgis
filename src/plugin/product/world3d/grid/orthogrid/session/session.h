// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_ORTHOGRID_SESSION_H_
#define PLUGIN_WORLD3D_ORTHOGRID_SESSION_H_

#include <string>

#include "plugin/product/world3d/grid/orthogrid/solve/boundary_solve.h"

namespace plugin {
namespace detail {

// Digitizing session for four-edge orthogrid (flags 0..3) plus mesh commit.
bool orthogrid_edges_complete();
BoundarySolve solve_orthogrid_session();
bool commit_orthogrid_solved(const BoundarySolve& solved);
bool write_orthogrid_gridbnd(const std::string& path);
int session_elliptic_iters();
void set_session_elliptic_iters(int n);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_ORTHOGRID_SESSION_H_

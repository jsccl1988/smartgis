// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_ORTHOGRID_SESSION_H_
#define PLUGIN_WORLD3D_ORTHOGRID_SESSION_H_

#include <string>
#include <string_view>

#include "plugin/product/world3d/scene/orthogrid/solve/boundary_solve.h"

namespace content {
class PluginHost;
}

namespace plugin {

void bind_orthogrid_present_host(content::PluginHost* host);
bool orthogrid_present_frame(content::PluginHost* host,
                             std::string_view args_json);
void arm_grid_boundary(int flag);

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

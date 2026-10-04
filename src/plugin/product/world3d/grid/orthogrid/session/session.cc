// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/orthogrid/session/session.h"

#include <cstddef>
#include <algorithm>
#include <fstream>
#include <utility>
#include <vector>

#include "plugin/product/world3d/commands.h"
#include "plugin/runtime/host/processing/operation_result.h"

namespace plugin {
namespace {

OrthogridMeshWriter g_mesh_writer;
int g_elliptic_iters = 0;
constexpr int kAutoMaxNodes = 64 * 64;

struct StoredEdge {
  int flag = 0;
  std::vector<std::pair<double, double>> pts;
};

std::vector<StoredEdge> g_edges;
int g_armed_flag = -1;

bool edges_complete() {
  bool seen[4] = {};
  for (const StoredEdge& e : g_edges) {
    if (e.flag >= 0 && e.flag <= 3) {
      seen[e.flag] = true;
    }
  }
  return seen[0] && seen[1] && seen[2] && seen[3];
}

int estimate_nodes() {
  int nx = 3;
  int ny = 3;
  for (const StoredEdge& edge : g_edges) {
    const int n = static_cast<int>(edge.pts.size());
    if (edge.flag == 0 || edge.flag == 2) {
      nx = std::max(nx, std::max(n, 3));
    } else {
      ny = std::max(ny, std::max(n, 3));
    }
  }
  return nx * ny;
}

bool commit_solved(const detail::BoundarySolve& solved) {
  if (!solved.ok) {
    return false;
  }
  if (!g_mesh_writer) {
    return true;
  }
  OrthogridMeshCommit commit;
  commit.nx = solved.nx;
  commit.ny = solved.ny;
  commit.xs = solved.xs.data();
  commit.ys = solved.ys.data();
  if (!solved.cell_orth.empty() &&
      solved.cell_orth.size() ==
          static_cast<size_t>((solved.nx - 1) * (solved.ny - 1))) {
    commit.cell_orth = solved.cell_orth.data();
  }
  if (!solved.raster_orth.empty() && solved.raster_w > 0 &&
      solved.raster_h > 0 &&
      solved.raster_orth.size() ==
          static_cast<size_t>(solved.raster_w * solved.raster_h)) {
    commit.raster_w = solved.raster_w;
    commit.raster_h = solved.raster_h;
    commit.raster_min_x = solved.raster_min_x;
    commit.raster_min_y = solved.raster_min_y;
    commit.raster_max_x = solved.raster_max_x;
    commit.raster_max_y = solved.raster_max_y;
    commit.raster_orth = solved.raster_orth.data();
  }
  commit.frame_count = static_cast<int>(solved.frame_xs.size());
  commit.frame_xs = solved.frame_xs.empty() ? nullptr : solved.frame_xs.data();
  commit.frame_ys = solved.frame_ys.empty() ? nullptr : solved.frame_ys.data();
  return g_mesh_writer(commit);
}

detail::BoundarySolve solve_session() {
  std::vector<detail::BoundaryEdge> edges;
  edges.reserve(g_edges.size());
  for (const StoredEdge& e : g_edges) {
    detail::BoundaryEdge be;
    be.flag = e.flag;
    be.pts = e.pts;
    edges.push_back(std::move(be));
  }
  return detail::solve_grid_boundary(edges, 0, 0, g_elliptic_iters);
}

bool maybe_auto_generate() {
  if (!edges_complete() || g_elliptic_iters != 0 ||
      estimate_nodes() > kAutoMaxNodes) {
    return false;
  }
  const detail::BoundarySolve solved = solve_session();
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  if (!commit_solved(solved)) {
    set_operation_result("{\"error\":\"mesh_commit_failed\"}");
    return false;
  }
  set_operation_result(solved.message);
  return true;
}

bool write_gridbnd(const std::string& path) {
  if (path.empty() || g_edges.empty()) {
    return false;
  }
  int nx = 3;
  int ny = 3;
  for (const StoredEdge& edge : g_edges) {
    const int n = static_cast<int>(edge.pts.size());
    if (edge.flag == 0 || edge.flag == 2) {
      nx = std::max(nx, std::max(n, 3));
    } else {
      ny = std::max(ny, std::max(n, 3));
    }
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out << "gridbnd:\n" << nx << " " << ny << "\nbegin\nmain_begin\n";
  for (const StoredEdge& edge : g_edges) {
    const int n = static_cast<int>(edge.pts.size());
    out << n << "\n";
    int start = 0;
    int end = 0;
    int index = 0;
    if (edge.flag == 0) {
      start = 0;
      end = nx - 1;
      index = 0;
    } else if (edge.flag == 1) {
      start = 0;
      end = ny - 1;
      index = nx - 1;
    } else if (edge.flag == 2) {
      start = nx - 1;
      end = 0;
      index = ny - 1;
    } else {
      start = ny - 1;
      end = 0;
      index = 0;
    }
    out << start << " " << end << " " << index << " " << edge.flag
        << " 1 1\n";
    for (const auto& pt : edge.pts) {
      out << pt.first << "," << pt.second << "\n";
    }
  }
  out << "main_end\nend\n";
  return static_cast<bool>(out);
}

}  // namespace

void set_orthogrid_mesh_writer(OrthogridMeshWriter writer) {
  g_mesh_writer = std::move(writer);
}

bool publish_orthogrid_mesh(const OrthogridMeshCommit& commit) {
  if (!g_mesh_writer) {
    return false;
  }
  return g_mesh_writer(commit);
}

void set_orthogrid_elliptic_iters(int n) { g_elliptic_iters = std::max(0, n); }

int orthogrid_elliptic_iters() { return g_elliptic_iters; }

void arm_grid_boundary(int flag) { g_armed_flag = flag; }

bool grid_boundary_armed() { return g_armed_flag >= 0; }

bool note_grid_boundary(const double* xy, size_t count) {
  if (g_armed_flag < 0 || !xy || count < 2) {
    return false;
  }
  StoredEdge edge;
  edge.flag = g_armed_flag;
  edge.pts.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    edge.pts.push_back({xy[i * 2], xy[i * 2 + 1]});
  }
  g_armed_flag = -1;
  for (StoredEdge& existing : g_edges) {
    if (existing.flag == edge.flag) {
      existing = std::move(edge);
      (void)maybe_auto_generate();
      return true;
    }
  }
  g_edges.push_back(std::move(edge));
  (void)maybe_auto_generate();
  return true;
}

namespace detail {

bool orthogrid_edges_complete() { return edges_complete(); }

BoundarySolve solve_orthogrid_session() { return solve_session(); }

bool commit_orthogrid_solved(const BoundarySolve& solved) {
  return commit_solved(solved);
}

bool write_orthogrid_gridbnd(const std::string& path) {
  return write_gridbnd(path);
}

int session_elliptic_iters() { return g_elliptic_iters; }

void set_session_elliptic_iters(int n) { g_elliptic_iters = std::max(0, n); }

}  // namespace detail
}  // namespace plugin

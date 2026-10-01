// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_
#define PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_

#include <string>
#include <utility>
#include <vector>

namespace plugin {
namespace detail {

// One boundary polyline painted onto a logical grid edge (flag 0..3).
struct BoundaryEdge {
  int flag = 0;
  std::vector<std::pair<double, double>> pts;
};

// Product-side gridbnd load + Dirichlet Laplace (+ optional Thompson steps).
// Does not link the 2010 Orthogrid class.
struct BoundarySolve {
  bool ok = false;
  int nx = 0;
  int ny = 0;
  int node_count = 0;
  int elliptic_iters = 0;
  std::vector<double> xs;
  std::vector<double> ys;
  // |90-theta| degrees; empty on failure.
  std::vector<float> node_orth;
  std::vector<float> cell_orth;
  // Axis-aligned heat underlay samples (optional; filled when ok).
  int raster_w = 0;
  int raster_h = 0;
  double raster_min_x = 0.0;
  double raster_min_y = 0.0;
  double raster_max_x = 0.0;
  double raster_max_y = 0.0;
  std::vector<float> raster_orth;
  // Intermediate grids after Laplace and each elliptic step (for playback).
  std::vector<std::vector<double>> frame_xs;
  std::vector<std::vector<double>> frame_ys;
  // JSON the processing host already forwards.
  std::string message;
};

// Solve from four edges. nx/ny < 3 → derived from edge point counts.
BoundarySolve solve_grid_boundary(const std::vector<BoundaryEdge>& edges,
                                  int nx,
                                  int ny,
                                  int elliptic_iters);

BoundarySolve solve_grid_boundary_file(const std::string& path,
                                       int elliptic_iters = 0);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID_BOUNDARY_SOLVE_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_COMMANDS_H_
#define PLUGIN_ORTHOGRID_COMMANDS_H_

#include <cstddef>
#include <functional>
#include <vector>

namespace content {
class PluginHost;
}

namespace plugin {

// Solved mesh + dual orthogonality heat fields for MapScene commit.
struct OrthogridMeshCommit {
  int nx = 0;
  int ny = 0;
  const double* xs = nullptr;
  const double* ys = nullptr;
  // Cell |90-theta|; size (nx-1)*(ny-1). Optional.
  const float* cell_orth = nullptr;
  int raster_w = 0;
  int raster_h = 0;
  double raster_min_x = 0.0;
  double raster_min_y = 0.0;
  double raster_max_x = 0.0;
  double raster_max_y = 0.0;
  // Axis-aligned heat samples; size raster_w*raster_h. Optional.
  const float* raster_orth = nullptr;
  // Intermediate grids (Laplace + each elliptic step). Optional.
  int frame_count = 0;
  const std::vector<double>* frame_xs = nullptr;
  const std::vector<double>* frame_ys = nullptr;
};

using OrthogridMeshWriter = std::function<bool(const OrthogridMeshCommit&)>;
void set_orthogrid_mesh_writer(OrthogridMeshWriter writer);
bool publish_orthogrid_mesh(const OrthogridMeshCommit& commit);

// Registers boundary input/save/load/generate and orth-grid processing.
bool register_orthogrid(content::PluginHost* host);

// Digitizing arms a flag (0..3). The next linestring the shell notes is
// stored; save_boundary writes gridbnd text.
void arm_grid_boundary(int flag);
bool grid_boundary_armed();
bool note_grid_boundary(const double* xy, size_t count);

// Session knobs used by auto-generate and processing.
void set_orthogrid_elliptic_iters(int n);
int orthogrid_elliptic_iters();

}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID_COMMANDS_H_

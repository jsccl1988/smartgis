// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_COMMANDS_H_
#define PLUGIN_WORLD3D_COMMANDS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ogr_geometry.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Solved mesh + dual orthogonality heat fields for GisScene commit.
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

bool publish_orthogrid_mesh(const OrthogridMeshCommit& commit);

// Digitizing arms a flag (0..3). The next linestring the shell notes is
// stored; save_boundary writes gridbnd text.
void arm_grid_boundary(int flag);
bool grid_boundary_armed();
bool note_grid_boundary(const double* xy, size_t count);

void set_orthogrid_elliptic_iters(int n);
int orthogrid_elliptic_iters();

// Solved volume mesh for Scene3D / analysis commit.
struct HexGridCommit {
  const OGRMultiPoint* nodes = nullptr;
  int nx = 0;
  int ny = 0;
  int nz = 0;
  const float* cell_orth = nullptr;
  int cell_orth_count = 0;
};

bool publish_hex_grid(const HexGridCommit& commit);

// Host façade: wires DEM, True-Earth scene, 2D orthogrid, and 3D hex layers.
// Command ids stay baogrid.* / orthogrid.* / orthogrid3d.* / model3d.* /
// world3d.* for AM, harness, and host_test. Pack ensure for those prefixes is
// owned by scenario/register (ensure_world3d_pack); this is the scene half.
bool register_world3d(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_COMMANDS_H_

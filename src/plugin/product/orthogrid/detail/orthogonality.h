// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_ORTHOGONALITY_H_
#define PLUGIN_ORTHOGRID_ORTHOGONALITY_H_

#include <cstddef>
#include <vector>

#include "plugin/product/orthogrid/detail/laplace_solver.h"

namespace orthogrid {

// Discrete orthogonality |90° − θ| in degrees (legacy CalGridOrthogonality).
// node_delta size = nx*ny (border nodes 0); cell_delta size = (nx-1)*(ny-1).
struct OrthogonalityField {
  std::vector<float> node_delta;
  std::vector<float> cell_delta;
};

// Heat class string "0".."3" (legacy stepped filters; prefer format_heat_delta).
const char* heat_class_from_delta(float delta_deg);

// Writes |90−θ| degrees as a decimal string for StyleDocument
// interpolate(["get","heat"], ...) color ramps. Returns buf, or "" on error.
const char* format_heat_delta(float delta_deg, char* buf, size_t cap);

OrthogonalityField compute_orthogonality(const GridField& grid);

// Axis-aligned raster samples over the node MBR (bilinear from node_delta).
// Out size = raster_w * raster_h. Returns false on bad args.
bool sample_orthogonality_raster(const GridField& grid,
                                 const float* node_delta,
                                 int raster_w,
                                 int raster_h,
                                 std::vector<float>* out_delta,
                                 double* min_x,
                                 double* min_y,
                                 double* max_x,
                                 double* max_y);

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_ORTHOGONALITY_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_ORTHO_LATTICE_H_
#define PLUGIN_WORLD3D_ORTHO_LATTICE_H_

#include "ogr_geometry.h"

namespace plugin {
namespace detail {

// Plugin-local XY structured lattice. Index = j * nx + i (i = col, j = row).
// Not exported from gis.dll; OGR has no Grid type.
struct OrthoLattice {
  OGRMultiPoint nodes;
  int nx = 0;
  int ny = 0;

  int node_count() const { return nx * ny; }
  bool is_empty() const {
    return nx < 1 || ny < 1 || nodes.IsEmpty();
  }

  int ortho_index(int i, int j) const;
  void ortho_resize(int nx_in, int ny_in);
  bool ortho_point(int i, int j, double* x, double* y) const;
  void ortho_set_point(int i, int j, double x, double y);
};

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_ORTHO_LATTICE_H_

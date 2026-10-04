// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEX_LATTICE_H_
#define PLUGIN_WORLD3D_HEX_LATTICE_H_

#include "ogr_geometry.h"

namespace plugin {
namespace detail {

// Plugin-local XYZ structured lattice. Index = k * ny * nx + j * nx + i.
// Not exported from gis.dll; OGR has no HexGrid type.
struct Xyz {
  double x = 0;
  double y = 0;
  double z = 0;
  Xyz() = default;
  Xyz(double x_in, double y_in, double z_in) : x(x_in), y(y_in), z(z_in) {}
};

struct HexLattice {
  OGRMultiPoint nodes;
  int nx = 0;
  int ny = 0;
  int nz = 0;

  int node_count() const { return nx * ny * nz; }
  bool is_empty() const {
    return nx < 1 || ny < 1 || nz < 1 || nodes.IsEmpty();
  }

  int hex_index(int i, int j, int k) const;
  void hex_resize(int nx_in, int ny_in, int nz_in);
  bool hex_point(int i, int j, int k, double* x, double* y, double* z) const;
  void hex_set_point(int i, int j, int k, double x, double y, double z);
};

// Free helpers for callers that hold OGRMultiPoint + dims (HexGridCommit).
inline int hex_index(int nx, int ny, int nz, int i, int j, int k) {
  if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) {
    return -1;
  }
  return k * ny * nx + j * nx + i;
}
bool hex_point(const OGRMultiPoint& nodes, int nx, int ny, int nz, int i,
               int j, int k, double* x, double* y, double* z);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_HEX_LATTICE_H_

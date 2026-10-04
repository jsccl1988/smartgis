// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_GRID_LAPLACE_H_
#define GIS_GEO_GRID_LAPLACE_H_

#include "gis/gis_export.h"

#include <cstdint>

namespace geo {

// Row-major XY node field: index = j * nx + i. Not an OGR type.
struct NodeField2d {
  int nx = 0;
  int ny = 0;
  double* x = nullptr;
  double* y = nullptr;
};

// Row-major XYZ node field: index = k * ny * nx + j * nx + i.
struct NodeField3d {
  int nx = 0;
  int ny = 0;
  int nz = 0;
  double* x = nullptr;
  double* y = nullptr;
  double* z = nullptr;
};

namespace detail {

inline int node_index_2d(int nx, int i, int j) {
  return j * nx + i;
}

inline int node_index_3d(int nx, int ny, int i, int j, int k) {
  return k * ny * nx + j * nx + i;
}

}  // namespace detail

// Discrete 5-point Laplace on a structured 2D lattice (Dirichlet where
// is_unknown[j*nx+i] == 0). Requires nx,ny >= 3.
GIS_EXPORT bool solve_laplace(NodeField2d field, const std::uint8_t* is_unknown);

// Discrete 7-point Laplace on a structured 3D lattice (Dirichlet where
// is_unknown[k*ny*nx+j*nx+i] == 0). Requires nx,ny,nz >= 3.
GIS_EXPORT bool solve_laplace(NodeField3d field, const std::uint8_t* is_unknown);

// One or more frozen-coefficient Thompson elliptic steps (α, β, γ from the
// current grid; control functions P = Q = 0). iters <= 0 is a no-op success.
// TFI, clustering P/Q, Thomas–Middlecoff, and 3D Thompson are not implemented.
GIS_EXPORT bool solve_elliptic(NodeField2d field,
                               const std::uint8_t* is_unknown,
                               int iters = 1);

}  // namespace geo

#endif  // GIS_GEO_GRID_LAPLACE_H_

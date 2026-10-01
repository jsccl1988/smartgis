// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID3D_ORTHOGONALITY_H_
#define PLUGIN_ORTHOGRID3D_ORTHOGONALITY_H_

#include <vector>

#include "plugin/product/orthogrid3d/detail/laplace_solver.h"

namespace orthogrid3d {

// Cell skew: mean of |90° − θ| over the three edge-pair angles at the
// cell corner (i,j,k) → (i+1,j,k),(i,j+1,k),(i,j,k+1). Size (nx-1)*(ny-1)*(nz-1).
struct OrthogonalityField3d {
  std::vector<float> cell_delta;
};

OrthogonalityField3d compute_orthogonality_3d(const VolumeField& field);

}  // namespace orthogrid3d

#endif  // PLUGIN_ORTHOGRID3D_ORTHOGONALITY_H_

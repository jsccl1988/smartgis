// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID3D_VTK_STRUCTURED_H_
#define PLUGIN_ORTHOGRID3D_VTK_STRUCTURED_H_

#include <string>
#include <vector>

#include "gis/kernel/geo/mesh/geometry.h"

namespace orthogrid3d {

// Write VTK XML StructuredGrid (.vts). Point order matches HexGrid index.
// Optional cell_orth size (nx-1)*(ny-1)*(nz-1); omitted when empty/nullptr.
bool write_vtk_structured(const geo::HexGrid& grid,
                          const std::string& path,
                          const float* cell_orth,
                          size_t cell_orth_count);

}  // namespace orthogrid3d

#endif  // PLUGIN_ORTHOGRID3D_VTK_STRUCTURED_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEXGRID_VTK_STRUCTURED_H_
#define PLUGIN_WORLD3D_HEXGRID_VTK_STRUCTURED_H_

#include <string>
#include <vector>

#include "plugin/product/world3d/grid/hexgrid/lattice/hex_lattice.h"

namespace plugin {
namespace detail {

// Write VTK XML StructuredGrid (.vts). Point order matches hex_index.
// Optional cell_orth size (nx-1)*(ny-1)*(nz-1); omitted when empty/nullptr.
bool write_vtk_structured(const HexLattice& grid,
                          const std::string& path,
                          const float* cell_orth,
                          size_t cell_orth_count);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_HEXGRID_VTK_STRUCTURED_H_

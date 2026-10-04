// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/hexgrid/io/vtk_structured.h"

#include <fstream>
#include <iomanip>

namespace plugin {
namespace detail {

bool write_vtk_structured(const HexLattice& grid,
                          const std::string& path,
                          const float* cell_orth,
                          size_t cell_orth_count) {
  if (grid.is_empty() || path.empty()) {
    return false;
  }
  const int nx = grid.nx;
  const int ny = grid.ny;
  const int nz = grid.nz;
  const int ex = nx - 1;
  const int ey = ny - 1;
  const int ez = nz - 1;
  const int cell_i = ex > 0 ? ex : 0;
  const int cell_j = ey > 0 ? ey : 0;
  const int cell_k = ez > 0 ? ez : 0;
  const size_t expected_cells =
      static_cast<size_t>(cell_i) * static_cast<size_t>(cell_j) *
      static_cast<size_t>(cell_k);
  const bool write_cells =
      cell_orth != nullptr && cell_orth_count == expected_cells &&
      expected_cells > 0;

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out << std::setprecision(17);
  out << "<?xml version=\"1.0\"?>\n";
  out << "<VTKFile type=\"StructuredGrid\" version=\"0.1\" "
         "byte_order=\"LittleEndian\">\n";
  out << "  <StructuredGrid WholeExtent=\"0 " << ex << " 0 " << ey << " 0 "
      << ez << "\">\n";
  out << "    <Piece Extent=\"0 " << ex << " 0 " << ey << " 0 " << ez
      << "\">\n";
  out << "      <Points>\n";
  out << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" "
         "format=\"ascii\">\n";
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        double x = 0;
        double y = 0;
        double z = 0;
        if (!grid.hex_point(i, j, k, &x, &y, &z)) {
          return false;
        }
        out << "          " << x << " " << y << " " << z << "\n";
      }
    }
  }
  out << "        </DataArray>\n";
  out << "      </Points>\n";
  if (write_cells) {
    out << "      <CellData Scalars=\"orthogonality\">\n";
    out << "        <DataArray type=\"Float32\" Name=\"orthogonality\" "
           "format=\"ascii\">\n";
    for (size_t n = 0; n < cell_orth_count; ++n) {
      out << "          " << cell_orth[n] << "\n";
    }
    out << "        </DataArray>\n";
    out << "      </CellData>\n";
  }
  out << "    </Piece>\n";
  out << "  </StructuredGrid>\n";
  out << "</VTKFile>\n";
  return static_cast<bool>(out);
}

}  // namespace detail
}  // namespace plugin

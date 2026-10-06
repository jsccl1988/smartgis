// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CONTOUR_MESH_H_
#define VISTA_TERRAIN_DEM_CONTOUR_MESH_H_

#include <cstdint>
#include <vector>

namespace vista {
namespace detail {

// Map DEM grid (col,row) → lon/lat. Row 0 = north / max-lat.
void grid_lon_lat(int cols, int rows, double minx, double miny, double maxx,
                  double maxy, float col, float row, double* lon, double* lat);

// Append one leftover Y-up mesh vertex (X=-lon, Y=elev, Z=lat).
void emit_mesh_xyz(double lon, float elev, double lat, std::vector<float>* xyz);

// Convert smoothed grid-space segments to mesh XYZ line segments at |elev|.
void emit_isoline_mesh_xyz(int cols, int rows, double minx, double miny,
                           double maxx, double maxy, float elev,
                           const std::vector<float>& segs,
                           std::vector<float>* xyz_segments);

// Regular-grid TIN with optional jet RGBA / UV (see build_contour_surface_tin).
bool build_surface_tin(const float* heights, int cols, int rows, double minx,
                       double miny, double maxx, double maxy, float z_offset,
                       float skip_below, float alpha, std::vector<float>* xyz,
                       std::vector<uint32_t>* indices, std::vector<float>* rgba,
                       std::vector<float>* uvs);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CONTOUR_MESH_H_

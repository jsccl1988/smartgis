// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_DEM_CONTOUR_H_
#define VISTA_TERRAIN_DEM_DEM_CONTOUR_H_

#include <cstdint>
#include <vector>

#include "vista/vista_export.h"

namespace vista {

// Origin-style jet ramp (purple → blue → cyan → green → yellow → red).
// |t01| is 0 at the low stop and 1 at the high stop.
VISTA_EXPORT void jet_elevation_rgb(float t01, float* r, float* g, float* b);

// Choose a round contour interval so ~8–14 levels cover |zmin|…|zmax|.
VISTA_EXPORT float pick_contour_interval_m(float zmin, float zmax);

// Bake an Origin-like elevation overlay from a row-major height grid
// (row 0 = north / max-lat). Ocean (h < 1 m) stays transparent.
// |surface| fills land with the jet ramp; |curves| strokes isolines.
// |interval_m| <= 0 picks a round interval from the land range.
VISTA_EXPORT bool bake_elevation_overlay_rgba(const float* heights, int cols,
                                              int rows, bool surface,
                                              bool curves, float interval_m,
                                              std::vector<uint8_t>* rgba);

// True-3D isoline segments on a geographic height grid (leftover Y-up mesh
// frame: X=-lon, Y=elev, Z=lat). Each segment contributes 6 floats
// (x0,y0,z0,x1,y1,z1). |z_offset| is added to every vertex Y. Cells where all
// four corners are < |skip_below| are skipped. |interval_m| <= 0 auto-picks.
VISTA_EXPORT bool extract_contour_curves_3d(const float* heights, int cols,
                                            int rows, double minx, double miny,
                                            double maxx, double maxy,
                                            float interval_m, float z_offset,
                                            float skip_below,
                                            std::vector<float>* xyz_segments);

// Regular-grid TIN (two triangles per cell) from the same height field, with
// per-vertex jet RGBA in 0..1 (|alpha| clamped). Mesh frame matches
// extract_contour_curves_3d. |z_offset| lifts the sheet for stacked display.
// Optional |uvs|: 2 floats/vert (u=col/(cols-1), v=1-row/(rows-1)) for drapes.
VISTA_EXPORT bool build_contour_surface_tin(
    const float* heights, int cols, int rows, double minx, double miny,
    double maxx, double maxy, float z_offset, float skip_below, float alpha,
    std::vector<float>* xyz, std::vector<uint32_t>* indices,
    std::vector<float>* rgba, std::vector<float>* uvs = nullptr);

}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_DEM_CONTOUR_H_

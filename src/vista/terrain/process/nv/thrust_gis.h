// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_PROCESS_NV_THRUST_GIS_H_
#define VISTA_TERRAIN_PROCESS_NV_THRUST_GIS_H_

#include <cstdint>

namespace vista {

// NVIDIA GIS note: RAPIDS cuSpatial is the spatial PIP library, but it is a
// Linux/conda RAPIDS stack and is not in this Windows GN tree. Product GPU
// path uses Thrust (NVIDIA CCC, Apache-2, CUDA Toolkit on Windows) for the
// same even-odd grid PIP and Horn hillshade. NPP has neither primitive.
// Returns false when CUDA/Thrust is unavailable or the launch fails — callers
// keep the CPU parallel_for path.

bool try_fill_lonlat_mask_thrust(double minx, double miny, double maxx,
                                 double maxy, int cols, int rows,
                                 const double* ring_x, const double* ring_y,
                                 const int* ring_off, int ring_count,
                                 int vert_count, uint8_t* out);

bool try_shade_dem_thrust(const float* heights, int cols, int rows, int step_x,
                          int step_y, int w, int h, float dx_m, float dy_m,
                          float exag, float az, float sin_alt, float cos_alt,
                          float sr, float sg, float sb, float hr, float hg,
                          float hb, uint8_t* rgba);

// Hypsometric albedo (+ coastal dilate when |land| is cols*rows). |land|
// may be null (all land, no dilate).
bool try_bake_hypso_thrust(const float* heights, const uint8_t* land, int cols,
                           int rows, int step_x, int step_y, int w, int h,
                           uint8_t* rgba);

// Jet fill on a packed height grid (ocean h < 1 stays A=0). Isolines stay CPU.
bool try_jet_fill_thrust(const float* heights, int cols, int rows, float zmin,
                         float zmax, uint8_t* rgba);

// True when this binary linked thrust_gis.cu (CUDA Toolkit present at compile).
bool thrust_gis_cuda_built();

}  // namespace vista

#endif  // VISTA_TERRAIN_PROCESS_NV_THRUST_GIS_H_

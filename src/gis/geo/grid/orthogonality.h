// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_GEO_GRID_ORTHOGONALITY_H_
#define GIS_GEO_GRID_ORTHOGONALITY_H_

#include "gis/geo/grid/laplace.h"
#include "gis/gis_export.h"

#include <vector>

namespace geo {

// Discrete 2D orthogonality |90° − θ| in degrees (centered ξ/η at interior
// nodes; cell values are the mean of the four corners).
struct Orthogonality2d {
  std::vector<float> node_delta;
  std::vector<float> cell_delta;
};

// Discrete 3D cell skew: mean of |90° − θ| over the three edge-pair angles
// at cell corner (i,j,k). Size (nx-1)*(ny-1)*(nz-1).
struct Orthogonality3d {
  std::vector<float> cell_delta;
};

GIS_EXPORT Orthogonality2d compute_orthogonality(NodeField2d field);
GIS_EXPORT Orthogonality3d compute_orthogonality(NodeField3d field);

// Axis-aligned raster samples over the node MBR (bilinear from node_delta).
// Out size = raster_w * raster_h. Returns false on bad args.
GIS_EXPORT bool sample_orthogonality_raster(NodeField2d field,
                                            const float* node_delta,
                                            int raster_w,
                                            int raster_h,
                                            std::vector<float>* out_delta,
                                            double* min_x,
                                            double* min_y,
                                            double* max_x,
                                            double* max_y);

}  // namespace geo

#endif  // GIS_GEO_GRID_ORTHOGONALITY_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_TRIMESH_LOADER_H_
#define PLUGIN_WORLD3D_TRIMESH_LOADER_H_

#include "plugin/product/world3d/world3d_export.h"
#include "gis/geo/ops/geometry_traits.h"
#include "ogr_geometry.h"

namespace plugin {

enum SeparatorType { ST_TAB, ST_SPACE, ST_COMMA };

// Column layout for an ASCII XYZ (or similar) point file.
struct TrimeshFileFmt {
  int nSeparatorType;
  int nCol;
  int iX, iY, iZ;
  int nHeadSkip;
  int nLineSkip;

  TrimeshFileFmt()
      : nCol(0),
        iX(0),
        iY(0),
        iZ(0),
        nSeparatorType(0),
        nHeadSkip(0),
        nLineSkip(0) {}

  TrimeshFileFmt(int n_col, int i_x, int i_y, int i_z, int separator_type,
                 int head_skip, int line_skip)
      : nCol(n_col),
        iX(i_x),
        iY(i_y),
        iZ(i_z),
        nSeparatorType(separator_type),
        nHeadSkip(head_skip),
        nLineSkip(line_skip) {}
};

// Parse ASCII XYZ into a point list, then Delaunay-mesh via geo::delaunay.
WORLD3D_LOADER_API long load_ascii_xyz_trimesh(const char* file_name,
                                               const TrimeshFileFmt& file_fmt,
                                               float x_scale, float y_scale,
                                               float z_scale,
                                               OGRTriangulatedSurface* out_surf);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_TRIMESH_LOADER_H_

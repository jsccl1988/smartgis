// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_DEM_TIN_LOADER_H
#define PLUGIN_DEM_TIN_LOADER_H

#include "algorithm/geo/geometry.h"

#if !defined(DEM_LOADER_API)
#if defined(PLUGIN_DEM_EXPORTS)
#define DEM_LOADER_API __declspec(dllexport)
#else
#define DEM_LOADER_API
#endif
#endif

using namespace geo;

namespace plugin {

enum SeparatorType { ST_TAB, ST_SPACE, ST_COMMA };

// Column layout for an ASCII XYZ (or similar) point file.
struct TinFileFmt {
  int nSeparatorType;
  int nCol;
  int iX, iY, iZ;
  int nHeadSkip;
  int nLineSkip;

  TinFileFmt()
      : nCol(0),
        iX(0),
        iY(0),
        iZ(0),
        nSeparatorType(0),
        nHeadSkip(0),
        nLineSkip(0) {}

  TinFileFmt(int n_col, int i_x, int i_y, int i_z, int separator_type,
             int head_skip, int line_skip)
      : nCol(n_col),
        iX(i_x),
        iY(i_y),
        iZ(i_z),
        nSeparatorType(separator_type),
        nHeadSkip(head_skip),
        nLineSkip(line_skip) {}
};

// Leftover MFC dlg_* still use the old type name.
using SmtTinFileFmt = TinFileFmt;

// Parse ASCII XYZ into a point list, then Delaunay-mesh via
// create_delaunay_tin_div.
DEM_LOADER_API long load_ascii_xyz_tin(const char* file_name,
                                       const SmtTinFileFmt& file_fmt,
                                       float x_scale, float y_scale,
                                       float z_scale, Smt3DSurface* out_surf);

}  // namespace plugin

#endif  // PLUGIN_DEM_TIN_LOADER_H

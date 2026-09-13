// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/dem/grid_loader.h"

#include <vector>

#include "base/core/core.h"
#include "gdal_priv.h"

namespace plugin {
namespace {

bool is_identity_geotransform(const double* gt) {
  return gt[0] == 0.0 && gt[1] == 1.0 && gt[2] == 0.0 && gt[3] == 0.0 &&
         gt[4] == 0.0 && gt[5] == 1.0;
}

long build_grid_surface(const std::vector<float>& heights,
                        int n_x,
                        int n_y,
                        const GridLoadOptions& options,
                        const double* geotransform,
                        bool use_geotransform,
                        Smt3DSurface* out_surf) {
  const int n_dots = n_y * n_x;
  const int n_tris = (n_y - 1) * (n_x - 1) * 2;

  OGRPoint* points = new OGRPoint[n_dots];
  base::Smt3DTriangle* tris = new base::Smt3DTriangle[n_tris];

  for (int i_y = 0; i_y < n_y; ++i_y) {
    // GDAL row 0 is the top of the raster. For dialog start/scale, walk
    // south-to-north so world Y matches the former BMP height-map.
    const int src_row = use_geotransform ? i_y : (n_y - 1 - i_y);
    const int row_base = i_y * n_x;
    for (int i_x = 0; i_x < n_x; ++i_x) {
      float f_x = 0.f;
      float f_y = 0.f;
      if (use_geotransform) {
        const double col = static_cast<double>(i_x) + 0.5;
        const double row = static_cast<double>(src_row) + 0.5;
        f_x = static_cast<float>(geotransform[0] + col * geotransform[1] +
                                 row * geotransform[2]);
        f_y = static_cast<float>(geotransform[3] + col * geotransform[4] +
                                 row * geotransform[5]);
      } else {
        f_x = i_x * options.x_scale + options.x_start;
        f_y = i_y * options.y_scale + options.y_start;
      }
      const float f_z =
          heights[static_cast<size_t>(src_row) * static_cast<size_t>(n_x) +
                  static_cast<size_t>(i_x)] *
              options.z_scale +
          options.z_start;
      points[row_base + i_x].setX(f_x);
      points[row_base + i_x].setY(f_y);
      points[row_base + i_x].setZ(f_z);
    }
  }

  int tri_index = 0;
  for (int i_y = 0; i_y < n_y - 1; ++i_y) {
    const int row = i_y * n_x;
    const int row_up = (i_y + 1) * n_x;
    for (int i_x = 0; i_x < n_x - 1; ++i_x) {
      tris[tri_index].a = row + i_x;
      tris[tri_index].b = row + i_x + 1;
      tris[tri_index].c = row_up + i_x + 1;
      ++tri_index;

      tris[tri_index].a = row + i_x;
      tris[tri_index].b = row_up + i_x + 1;
      tris[tri_index].c = row_up + i_x;
      ++tri_index;
    }
  }

  out_surf->is_empty();
  out_surf->add_point_collection(points, n_dots);
  out_surf->add_triangle_collection(tris, n_tris);

  SMT_SAFE_DELETE_A(tris);
  SMT_SAFE_DELETE_A(points);
  return SMT_ERR_NONE;
}

}  // namespace

long load_heightmap_grid(const char* file_name,
                         const GridLoadOptions& options,
                         Smt3DSurface* out_surf) {
  if (!file_name || !out_surf)
    return SMT_ERR_FAILURE;

  GDALAllRegister();
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(file_name, GA_ReadOnly));
  if (!dataset)
    return SMT_ERR_INVALID_FILE;

  const int n_x = dataset->GetRasterXSize();
  const int n_y = dataset->GetRasterYSize();
  GDALRasterBand* band = dataset->GetRasterBand(1);
  if (!band || n_x < 2 || n_y < 2) {
    GDALClose(dataset);
    return SMT_ERR_INVALID_FILE;
  }

  double geotransform[6] = {0};
  const bool use_geotransform =
      dataset->GetGeoTransform(geotransform) == CE_None &&
      !is_identity_geotransform(geotransform);

  std::vector<float> heights(static_cast<size_t>(n_x) * static_cast<size_t>(n_y));
  const CPLErr err = band->RasterIO(GF_Read, 0, 0, n_x, n_y, heights.data(), n_x,
                                    n_y, GDT_Float32, 0, 0);
  GDALClose(dataset);
  if (err != CE_None)
    return SMT_ERR_INVALID_FILE;

  return build_grid_surface(heights, n_x, n_y, options, geotransform,
                            use_geotransform, out_surf);
}

}  // namespace plugin

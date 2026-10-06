// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/dem/loader/heightmap_loader.h"

#include <vector>

#include "gdal_priv.h"
#include "gis/geo/ops/geometry_traits.h"
#include "gis/geo/ops/indexed_tin.h"

namespace plugin {
namespace {

bool is_identity_geotransform(const double* gt) {
  return gt[0] == 0.0 && gt[1] == 1.0 && gt[2] == 0.0 && gt[3] == 0.0 &&
         gt[4] == 0.0 && gt[5] == 1.0;
}

long build_heightmap_surface(const std::vector<float>& heights,
                        int n_x,
                        int n_y,
                        const HeightmapLoadOptions& options,
                        const double* geotransform,
                        bool use_geotransform,
                        OGRTriangulatedSurface* out_surf) {
  const int n_dots = n_y * n_x;
  const int n_tris = (n_y - 1) * (n_x - 1) * 2;

  std::vector<geo::Vertex3> points(static_cast<size_t>(n_dots));
  std::vector<geo::IndexedTriangle> tris(static_cast<size_t>(n_tris));

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
      points[static_cast<size_t>(row_base + i_x)] =
          geo::Vertex3(f_x, f_y, f_z);
    }
  }

  int tri_index = 0;
  for (int i_y = 0; i_y < n_y - 1; ++i_y) {
    const int row = i_y * n_x;
    const int row_up = (i_y + 1) * n_x;
    for (int i_x = 0; i_x < n_x - 1; ++i_x) {
      tris[static_cast<size_t>(tri_index)].a = row + i_x;
      tris[static_cast<size_t>(tri_index)].b = row + i_x + 1;
      tris[static_cast<size_t>(tri_index)].c = row_up + i_x + 1;
      ++tri_index;

      tris[static_cast<size_t>(tri_index)].a = row + i_x;
      tris[static_cast<size_t>(tri_index)].b = row_up + i_x + 1;
      tris[static_cast<size_t>(tri_index)].c = row_up + i_x;
      ++tri_index;
    }
  }

  if (!geo::fill_indexed_tin(out_surf, points, tris)) {
    return geo::k_fail;
  }
  return geo::k_ok;
}

}  // namespace

long load_heightmap(const char* file_name,
                    const HeightmapLoadOptions& options,
                    OGRTriangulatedSurface* out_surf) {
  if (!file_name || !out_surf)
    return geo::k_fail;

  GDALAllRegister();
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(file_name, GA_ReadOnly));
  if (!dataset)
    return geo::k_fail;

  const int n_x = dataset->GetRasterXSize();
  const int n_y = dataset->GetRasterYSize();
  GDALRasterBand* band = dataset->GetRasterBand(1);
  if (!band || n_x < 2 || n_y < 2) {
    GDALClose(dataset);
    return geo::k_fail;
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
    return geo::k_fail;

  return build_heightmap_surface(heights, n_x, n_y, options, geotransform,
                                 use_geotransform, out_surf);
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/field_ingest.h"

#include "gdal_priv.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gis {
namespace atmosphere {

bool ingest_gdal_field(FieldStore* store, const char* path,
                       FieldChannel channel, const FieldIngestOptions& opts) {
  if (!store || !path || !path[0]) {
    return false;
  }
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path, GA_ReadOnly));
  if (!ds) {
    return false;
  }
  const int n_x = ds->GetRasterXSize();
  const int n_y = ds->GetRasterYSize();
  if (n_x < 1 || n_y < 1 || opts.band < 1 ||
      opts.band > ds->GetRasterCount()) {
    GDALClose(ds);
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(opts.band);
  if (!band) {
    GDALClose(ds);
    return false;
  }

  FieldGrid grid;
  grid.cols = n_x;
  grid.rows = n_y;
  double gt[6] = {};
  bool north_up = false;
  if (ds->GetGeoTransform(gt) == CE_None) {
    north_up = gt[5] < 0.0;
    // Pixel-corner geotransform → node envelope spanning the raster.
    const double x0 = gt[0];
    const double y0 = gt[3];
    const double x1 = gt[0] + gt[1] * n_x + gt[2] * n_y;
    const double y1 = gt[3] + gt[4] * n_x + gt[5] * n_y;
    grid.min_lon = (std::min)(x0, x1);
    grid.max_lon = (std::max)(x0, x1);
    grid.min_lat = (std::min)(y0, y1);
    grid.max_lat = (std::max)(y0, y1);
  } else {
    grid.min_lon = 0.0;
    grid.min_lat = 0.0;
    grid.max_lon = static_cast<double>(n_x);
    grid.max_lat = static_cast<double>(n_y);
  }

  std::vector<float> values(grid.cell_count());
  const CPLErr err =
      band->RasterIO(GF_Read, 0, 0, n_x, n_y, values.data(), n_x, n_y,
                     GDT_Float32, 0, 0);
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  GDALClose(ds);
  if (err != CE_None) {
    return false;
  }

  // FieldGrid row 0 is min_lat; north-up GeoTIFF row 0 is max_lat — flip.
  if (north_up && n_y > 1) {
    std::vector<float> flipped(values.size());
    for (int row = 0; row < n_y; ++row) {
      const int src = row;
      const int dst = n_y - 1 - row;
      for (int col = 0; col < n_x; ++col) {
        flipped[static_cast<std::size_t>(dst) * static_cast<std::size_t>(n_x) +
                static_cast<std::size_t>(col)] =
            values[static_cast<std::size_t>(src) * static_cast<std::size_t>(n_x) +
                   static_cast<std::size_t>(col)];
      }
    }
    values.swap(flipped);
  }

  std::vector<uint8_t> mask;
  if (nodata_ok) {
    mask.assign(values.size(), 1);
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (std::isnan(values[i]) ||
          std::abs(static_cast<double>(values[i]) - nodata) < 1e-6) {
        mask[i] = 0;
      }
    }
  }

  return store->upload_slice(
      channel, opts.kind, opts.priority, grid, values.data(), values.size(),
      mask.empty() ? nullptr : mask.data(), mask.size(), opts.time_sec);
}

bool ingest_gdal_field_series(FieldStore* store, FieldChannel channel,
                              const char* const* paths, const double* times,
                              std::size_t count,
                              const FieldIngestOptions& opts) {
  if (!store || !paths || !times || count == 0) {
    return false;
  }
  for (std::size_t i = 0; i < count; ++i) {
    if (!paths[i] || !paths[i][0]) {
      return false;
    }
    FieldIngestOptions slice_opts = opts;
    slice_opts.time_sec = times[i];
    if (!ingest_gdal_field(store, paths[i], channel, slice_opts)) {
      return false;
    }
  }
  return true;
}

}  // namespace atmosphere
}  // namespace gis

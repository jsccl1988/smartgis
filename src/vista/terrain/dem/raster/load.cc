// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_raster.h"

#include "base/time/elapsed_timer.h"
#include "vista/terrain/dem/dem_bake_cache.h"
#include "vista/terrain/dem/raster/synthetic.h"
#include "gdal_priv.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace vista {

bool DemRaster::adopt_bake_cache(int cols, int rows, double minx, double miny,
                                 double maxx, double maxy, float min_m,
                                 float max_m, float vert_exag,
                                 std::vector<float> heights,
                                 std::vector<uint8_t> land, const char* path) {
  if (cols < 2 || rows < 2 ||
      heights.size() !=
          static_cast<size_t>(cols) * static_cast<size_t>(rows)) {
    return false;
  }
  cols_ = cols;
  rows_ = rows;
  minx_ = minx;
  miny_ = miny;
  maxx_ = maxx;
  maxy_ = maxy;
  min_m_ = min_m;
  max_m_ = max_m;
  vert_exag_ = vert_exag;
  heights_ = std::move(heights);
  if (land.size() == heights_.size()) {
    land_ = std::move(land);
  } else {
    land_.assign(heights_.size(), 1);
  }
  source_path_ = (path && path[0]) ? path : "";
  return !empty();
}

void DemRaster::export_bake_cache(int* cols, int* rows, double* minx,
                                  double* miny, double* maxx, double* maxy,
                                  float* min_m, float* max_m, float* vert_exag,
                                  const std::vector<float>** heights,
                                  const std::vector<uint8_t>** land) const {
  if (cols) {
    *cols = cols_;
  }
  if (rows) {
    *rows = rows_;
  }
  if (minx) {
    *minx = minx_;
  }
  if (miny) {
    *miny = miny_;
  }
  if (maxx) {
    *maxx = maxx_;
  }
  if (maxy) {
    *maxy = maxy_;
  }
  if (min_m) {
    *min_m = min_m_;
  }
  if (max_m) {
    *max_m = max_m_;
  }
  if (vert_exag) {
    *vert_exag = vert_exag_;
  }
  if (heights) {
    *heights = &heights_;
  }
  if (land) {
    *land = &land_;
  }
}

bool DemRaster::load_gdal_raster(const char* path) {
  heights_.clear();
  land_.clear();
  cols_ = 0;
  rows_ = 0;
  source_path_.clear();
  if (!path || !path[0]) {
    return false;
  }
  {
    base::ElapsedTimer load_timer;
    if (dem_raster_cache_try_get(path, this)) {
      note_dem_phase_load(
          static_cast<int64_t>(load_timer.elapsed_milliseconds() + 0.5),
          /*cache_hit=*/true);
      return !empty();
    }
  }
  base::ElapsedTimer load_timer;
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path, GA_ReadOnly));
  if (!ds) {
    return false;
  }
  const int n_x = ds->GetRasterXSize();
  const int n_y = ds->GetRasterYSize();
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band || n_x < 2 || n_y < 2) {
    GDALClose(ds);
    return false;
  }
  double gt[6] = {};
  const bool has_gt = ds->GetGeoTransform(gt) == CE_None;
  std::vector<float> raw(static_cast<size_t>(n_x) * static_cast<size_t>(n_y));
  const CPLErr err = band->RasterIO(GF_Read, 0, 0, n_x, n_y, raw.data(), n_x,
                                    n_y, GDT_Float32, 0, 0);
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  GDALClose(ds);
  if (err != CE_None) {
    return false;
  }
  cols_ = n_x;
  rows_ = n_y;
  heights_.swap(raw);
  if (has_gt) {
    minx_ = gt[0];
    maxy_ = gt[3];
    maxx_ = gt[0] + gt[1] * n_x + gt[2] * n_y;
    miny_ = gt[3] + gt[4] * n_x + gt[5] * n_y;
    if (miny_ > maxy_) {
      std::swap(miny_, maxy_);
    }
    if (minx_ > maxx_) {
      std::swap(minx_, maxx_);
    }
  } else {
    minx_ = 0;
    miny_ = 0;
    maxx_ = static_cast<double>(n_x - 1);
    maxy_ = static_cast<double>(n_y - 1);
  }
  if (nodata_ok) {
    for (float& h : heights_) {
      if (h == static_cast<float>(nodata) || h < 0.f) {
        h = 0.f;
      }
    }
  } else {
    for (float& h : heights_) {
      if (h < 0.f) {
        h = 0.f;
      }
    }
  }
  // Real china_dem* already encodes land/ocean via nodata/≤0 heights. Build an
  // explicit land_ mask so bake paints green land (not ocean blue on every
  // cell) and mesh can stay land-only.
  if (land_.empty() && !heights_.empty()) {
    land_.assign(heights_.size(), 0);
    for (size_t i = 0; i < heights_.size(); ++i) {
      land_[i] = heights_[i] > 1.f ? 1 : 0;
    }
  }
  // Keep china_dem 1536×960 (AWS Mapzen z7). Mesh/bake LOD still downsamples
  // via max_edge; do not silently crush hi-res samples to the old 384×240 era.
  downsample_to_max_edge(1536);
  recompute_range();
  fit_vertical_exaggeration();
  source_path_ = path;
  if (!empty()) {
    dem_raster_cache_put(path, *this);
  }
  note_dem_phase_load(
      static_cast<int64_t>(load_timer.elapsed_milliseconds() + 0.5),
      /*cache_hit=*/false);
  return !empty();
}

void DemRaster::fill_synthetic_china() {
  source_path_.clear();
  cols_ = 320;
  rows_ = 200;
  minx_ = 73.0;
  maxx_ = 135.0;
  miny_ = 17.5;
  maxy_ = 54.0;
  land_.clear();
  heights_.assign(static_cast<size_t>(cols_ * rows_), 0.f);
  for (int row = 0; row < rows_; ++row) {
    const double lat =
        maxy_ - (static_cast<double>(row) + 0.5) * (maxy_ - miny_) / rows_;
    for (int col = 0; col < cols_; ++col) {
      const double lon =
          minx_ + (static_cast<double>(col) + 0.5) * (maxx_ - minx_) / cols_;
      heights_[static_cast<size_t>(index_at(col, row))] =
          detail::synthetic_meters(lon, lat);
    }
  }
  recompute_range();
  fit_vertical_exaggeration();
}

}  // namespace vista

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/dem_raster.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "base/time/elapsed_timer.h"
#include "vista/terrain/dem/dem_bake_cache.h"
#include "vista/terrain/dem/dem_contour.h"
#include "vista/terrain/process/bake_parallel.h"
#include "vista/terrain/process/nv/bake_pixel.h"
#include "vista/terrain/process/nv/thrust_gis.h"
#include "gdal_priv.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace vista {
namespace {

std::string& dem_path_override_store() {
  static std::string path;
  return path;
}

float clampf(float v, float lo, float hi) {
  return detail::bake_clampf(v, lo, hi);
}

template <typename Fn>
void for_each_bake_row(int w, int h, Fn fn) {
  if (bake_rows_should_parallel(w, h)) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, h, fn);
  } else {
    for (int row = 0; row < h; ++row) {
      fn(row);
    }
  }
}

float gauss_hill(double x, double y, double cx, double cy, double sx, double sy,
                 float peak) {
  const double dx = (x - cx) / sx;
  const double dy = (y - cy) / sy;
  return peak * static_cast<float>(std::exp(-0.5 * (dx * dx + dy * dy)));
}

float hash_noise(int ix, int iy) {
  unsigned h = static_cast<unsigned>(ix * 374761393u + iy * 668265263u);
  h = (h ^ (h >> 13)) * 1274126177u;
  return static_cast<float>(h & 0xffff) / 65535.f;
}

float synthetic_meters(double lon, double lat) {
  float m = 80.f;
  m += 2800.f * static_cast<float>(std::exp(
           -0.5 * ((lon - 90.0) / 14.0) * ((lon - 90.0) / 14.0) -
           0.5 * ((lat - 33.0) / 6.5) * ((lat - 33.0) / 6.5)));
  m += gauss_hill(lon, lat, 86.5, 28.0, 3.8, 1.8, 2200.f);
  m += gauss_hill(lon, lat, 91.0, 30.5, 5.5, 2.8, 1600.f);
  m += gauss_hill(lon, lat, 99.0, 28.5, 2.8, 2.2, 2400.f);
  m += gauss_hill(lon, lat, 102.5, 27.5, 2.2, 1.8, 1800.f);
  m += gauss_hill(lon, lat, 85.0, 42.5, 5.5, 1.5, 2200.f);
  m += gauss_hill(lon, lat, 88.0, 38.5, 6.0, 1.4, 1800.f);
  m += gauss_hill(lon, lat, 100.0, 38.0, 4.5, 1.6, 1400.f);
  m += gauss_hill(lon, lat, 107.5, 34.0, 3.5, 1.4, 900.f);
  m += gauss_hill(lon, lat, 112.5, 37.5, 1.8, 2.5, 700.f);
  m += gauss_hill(lon, lat, 127.5, 42.5, 2.0, 1.8, 900.f);
  m += gauss_hill(lon, lat, 121.0, 23.8, 0.55, 1.1, 2400.f);
  m += gauss_hill(lon, lat, 109.8, 19.0, 0.7, 0.5, 600.f);
  m += gauss_hill(lon, lat, 117.0, 27.5, 2.2, 1.6, 600.f);
  m -= gauss_hill(lon, lat, 84.0, 40.5, 4.5, 2.2, 2200.f);
  m -= gauss_hill(lon, lat, 87.0, 46.0, 3.5, 1.8, 1800.f);
  m -= gauss_hill(lon, lat, 105.5, 30.5, 2.5, 1.8, 1200.f);
  m -= gauss_hill(lon, lat, 116.5, 33.0, 6.0, 4.5, 400.f);
  m -= gauss_hill(lon, lat, 120.5, 31.5, 3.5, 2.0, 250.f);
  const int ix = static_cast<int>(std::floor(lon * 8.0));
  const int iy = static_cast<int>(std::floor(lat * 8.0));
  m += (hash_noise(ix, iy) - 0.5f) * 60.f;
  const bool taiwan = lon > 120.0 && lon < 122.3 && lat > 21.8 && lat < 25.5;
  const bool hainan = lon > 108.5 && lon < 111.2 && lat > 18.0 && lat < 20.2;
  const bool ocean = !taiwan && !hainan &&
                     ((lon > 122.4 && lat < 31.5) ||
                      (lon > 118.0 && lat < 21.5) || (lat < 17.5));
  if (ocean) {
    m = 0.f;
  }
  return clampf(m, 0.f, 8800.f);
}

std::string join_dir(const std::string& dir, const char* rel) {
  return dir + rel;
}

}  // namespace

// Base elevation ramp. Lowlands stay green (landish gate: g>r and g>b).
// Highlands are tan, not a pink-white poster. Slope/snow live in the bake.
void hypsometric_rgb(float meters, float* r, float* g, float* b) {
  detail::hypsometric_rgb_impl(meters, r, g, b);
}

void terrain_material_rgb(float meters, float slope01, float* r, float* g,
                          float* b) {
  detail::terrain_material_rgb_impl(meters, slope01, r, g, b);
}

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
          synthetic_meters(lon, lat);
    }
  }
  recompute_range();
  fit_vertical_exaggeration();
}

void DemRaster::fit_vertical_exaggeration() {
  const float span =
      static_cast<float>((std::max)(maxx_ - minx_, maxy_ - miny_));
  const float peak = (std::max)(80.f, max_m_ - min_m_);
  // Match leftover DemHeightField (span * 0.09 / peak): milder relief so
  // draped imagery and orbit pitch read like SmartGis.exe.
  vert_exag_ = (span * 0.09f) / peak;
}

float DemRaster::meters_at(int col, int row) const {
  col = (std::max)(0, (std::min)(cols_ - 1, col));
  row = (std::max)(0, (std::min)(rows_ - 1, row));
  return heights_[static_cast<size_t>(index_at(col, row))];
}

void DemRaster::recompute_range() {
  if (heights_.empty()) {
    min_m_ = 0;
    max_m_ = 1;
    return;
  }
  min_m_ = heights_[0];
  max_m_ = heights_[0];
  for (float h : heights_) {
    min_m_ = (std::min)(min_m_, h);
    max_m_ = (std::max)(max_m_, h);
  }
  if (max_m_ <= min_m_) {
    max_m_ = min_m_ + 1.f;
  }
}

void DemRaster::downsample_to_max_edge(int max_edge) {
  if (max_edge < 8 || empty()) {
    return;
  }
  const int long_edge = (std::max)(cols_, rows_);
  if (long_edge <= max_edge) {
    return;
  }
  const int new_cols = (std::max)(2, cols_ * max_edge / long_edge);
  const int new_rows = (std::max)(2, rows_ * max_edge / long_edge);
  std::vector<float> next(static_cast<size_t>(new_cols * new_rows));
  for (int row = 0; row < new_rows; ++row) {
    const int src_row = row * rows_ / new_rows;
    for (int col = 0; col < new_cols; ++col) {
      const int src_col = col * cols_ / new_cols;
      next[static_cast<size_t>(row * new_cols + col)] =
          meters_at(src_col, src_row);
    }
  }
  heights_.swap(next);
  cols_ = new_cols;
  rows_ = new_rows;
  // Rebuild land_ after resize. Clearing without rebuild left land_ empty so
  // bake treated every cell as land while mesh windows that later call
  // mask_outside_rings stayed coherent — but a post-downsample empty mask
  // also dropped the height>1 land cue used by hypsometric ocean navy.
  land_.assign(heights_.size(), 0);
  for (size_t i = 0; i < heights_.size(); ++i) {
    land_[i] = heights_[i] > 1.f ? 1 : 0;
  }
}

void DemRaster::mask_outside_rings(const std::vector<LonLatRing>& rings) {
  if (empty() || rings.empty()) {
    return;
  }
  land_.assign(static_cast<size_t>(cols_ * rows_), 0);
  fill_lonlat_mask(minx_, miny_, maxx_, maxy_, cols_, rows_, rings,
                   land_.data());
  for (size_t i = 0; i < heights_.size(); ++i) {
    if (!land_[i]) {
      heights_[i] = 0.f;
    }
  }
  recompute_range();
  fit_vertical_exaggeration();
}

float DemRaster::sample_meters(double x, double y) const {
  if (empty()) {
    return 0.f;
  }
  const double dx = maxx_ - minx_;
  const double dy = maxy_ - miny_;
  if (dx <= 0.0 || dy <= 0.0) {
    return 0.f;
  }
  const double u = (x - minx_) / dx * (cols_ - 1);
  const double v = (maxy_ - y) / dy * (rows_ - 1);
  const int c0 = static_cast<int>(std::floor(u));
  const int r0 = static_cast<int>(std::floor(v));
  const float tx = static_cast<float>(u - c0);
  const float ty = static_cast<float>(v - r0);
  const float h00 = meters_at(c0, r0);
  const float h10 = meters_at(c0 + 1, r0);
  const float h01 = meters_at(c0, r0 + 1);
  const float h11 = meters_at(c0 + 1, r0 + 1);
  const float h0 = h00 * (1.f - tx) + h10 * tx;
  const float h1 = h01 * (1.f - tx) + h11 * tx;
  return h0 * (1.f - ty) + h1 * ty;
}

float DemRaster::sample(double x, double y) const {
  return sample_meters(x, y) * vert_exag_;
}

void DemRaster::envelope(double* minx, double* miny, double* maxx,
                         double* maxy) const {
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
}

int DemRaster::lod_max_edge(float camera_distance, int min_edge,
                            int max_edge_cap) {
  if (min_edge < 8) {
    min_edge = 8;
  }
  if (max_edge_cap < min_edge) {
    max_edge_cap = min_edge;
  }
  // Orbit framing: distance ~2.55 is the full/land showcase — keep the dense
  // bucket (max_edge_cap) so coasts are not stair-stepped. Coarser LODs only
  // kick in past the overview (~3.0+).
  int edge = max_edge_cap;
  if (camera_distance > 8.0f) {
    edge = min_edge;
  } else if (camera_distance > 5.0f) {
    edge = min_edge + (max_edge_cap - min_edge) / 4;
  } else if (camera_distance > 3.6f) {
    edge = min_edge + (max_edge_cap - min_edge) / 2;
  } else if (camera_distance > 3.0f) {
    edge = min_edge + 3 * (max_edge_cap - min_edge) / 4;
  }
  if (edge < min_edge) {
    edge = min_edge;
  }
  if (edge > max_edge_cap) {
    edge = max_edge_cap;
  }
  return edge;
}

int DemRaster::lod_expected_vertices(int cols, int rows, int max_edge) {
  if (cols < 2 || rows < 2) {
    return 0;
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols > max_edge) {
      step_x = cols / max_edge;
    }
    if (rows > max_edge) {
      step_y = rows / max_edge;
    }
  }
  if (step_x < 1) {
    step_x = 1;
  }
  if (step_y < 1) {
    step_y = 1;
  }
  const int mc = (cols + step_x - 1) / step_x;
  const int mr = (rows + step_y - 1) / step_y;
  return mc * mr;
}

bool DemRaster::build_mesh(int max_edge, std::vector<float>* xyz,
                           std::vector<uint32_t>* indices) const {
  return build_mesh(max_edge, xyz, indices, nullptr);
}

bool DemRaster::build_mesh(int max_edge, std::vector<float>* xyz,
                           std::vector<uint32_t>* indices,
                           std::vector<float>* uvs) const {
  if (empty()) {
    return false;
  }
  return build_mesh_window(minx_, miny_, maxx_, maxy_, max_edge, xyz, indices,
                           uvs);
}

bool DemRaster::build_mesh_window(double minx, double miny, double maxx,
                                  double maxy, int max_edge,
                                  std::vector<float>* xyz,
                                  std::vector<uint32_t>* indices,
                                  std::vector<float>* uvs,
                                  bool apply_land_mask) const {
  if (!xyz || !indices || empty()) {
    return false;
  }
  // Intersect with DEM envelope.
  const double win_minx = (std::max)(minx, minx_);
  const double win_miny = (std::max)(miny, miny_);
  const double win_maxx = (std::min)(maxx, maxx_);
  const double win_maxy = (std::min)(maxy, maxy_);
  if (!(win_maxx > win_minx) || !(win_maxy > win_miny)) {
    return false;
  }
  const double dx = (maxx_ - minx_) / (std::max)(1, cols_ - 1);
  const double dy = (maxy_ - miny_) / (std::max)(1, rows_ - 1);
  int col0 = static_cast<int>(std::floor((win_minx - minx_) / dx));
  int col1 = static_cast<int>(std::ceil((win_maxx - minx_) / dx));
  int row0 = static_cast<int>(std::floor((maxy_ - win_maxy) / dy));
  int row1 = static_cast<int>(std::ceil((maxy_ - win_miny) / dy));
  col0 = (std::max)(0, (std::min)(cols_ - 1, col0));
  col1 = (std::max)(0, (std::min)(cols_ - 1, col1));
  row0 = (std::max)(0, (std::min)(rows_ - 1, row0));
  row1 = (std::max)(0, (std::min)(rows_ - 1, row1));
  if (col1 <= col0 || row1 <= row0) {
    return false;
  }
  const int win_cols = col1 - col0 + 1;
  const int win_rows = row1 - row0 + 1;
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (win_cols > max_edge) {
      step_x = win_cols / max_edge;
    }
    if (win_rows > max_edge) {
      step_y = win_rows / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int mc = (win_cols + step_x - 1) / step_x;
  const int mr = (win_rows + step_y - 1) / step_y;
  if (mc < 2 || mr < 2) {
    return false;
  }
  xyz->clear();
  indices->clear();
  if (uvs) {
    uvs->clear();
  }
  auto is_land = [&](int src_col, int src_row) -> bool {
    if (!apply_land_mask || land_.empty()) {
      return true;
    }
    src_col = (std::max)(0, (std::min)(cols_ - 1, src_col));
    src_row = (std::max)(0, (std::min)(rows_ - 1, src_row));
    return land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
  };
  std::vector<int> vert_of(static_cast<size_t>(mc * mr), -1);
  // UVs must address the full DEM hypsometric/drape texture (bake covers
  // cols_×rows_), not the local mesh window grid. Window-local 0..1 made
  // land-only meshes sample ocean texels → black FlyCube DEM slabs.
  const float u_den = static_cast<float>((std::max)(1, cols_ - 1));
  const float v_den = static_cast<float>((std::max)(1, rows_ - 1));
  std::vector<uint8_t> keep(static_cast<size_t>(mc * mr), 0);
  std::vector<int> row_n(static_cast<size_t>(mr), 0);
  auto mark_row = [&](int row) {
    const int src_row =
        (std::min)(row1, row0 + (std::min)(win_rows - 1, row * step_y));
    int n = 0;
    for (int col = 0; col < mc; ++col) {
      const int src_col =
          (std::min)(col1, col0 + (std::min)(win_cols - 1, col * step_x));
      if (!is_land(src_col, src_row)) {
        continue;
      }
      keep[static_cast<size_t>(row * mc + col)] = 1;
      ++n;
    }
    row_n[static_cast<size_t>(row)] = n;
  };
  for_each_bake_row(mc, mr, mark_row);
  std::vector<int> row_off(static_cast<size_t>(mr) + 1u, 0);
  for (int row = 0; row < mr; ++row) {
    row_off[static_cast<size_t>(row) + 1] =
        row_off[static_cast<size_t>(row)] + row_n[static_cast<size_t>(row)];
  }
  const int nverts = row_off[static_cast<size_t>(mr)];
  if (nverts < 3) {
    return false;
  }
  xyz->assign(static_cast<size_t>(nverts) * 3u, 0.f);
  if (uvs) {
    uvs->assign(static_cast<size_t>(nverts) * 2u, 0.f);
  }
  float* xyzp = xyz->data();
  float* uvp = uvs ? uvs->data() : nullptr;
  auto fill_row = [&](int row) {
    const int src_row =
        (std::min)(row1, row0 + (std::min)(win_rows - 1, row * step_y));
    const double lat = maxy_ - src_row * dy;
    int out = row_off[static_cast<size_t>(row)];
    for (int col = 0; col < mc; ++col) {
      if (!keep[static_cast<size_t>(row * mc + col)]) {
        continue;
      }
      const int src_col =
          (std::min)(col1, col0 + (std::min)(win_cols - 1, col * step_x));
      vert_of[static_cast<size_t>(row * mc + col)] = out;
      const double lon = minx_ + src_col * dx;
      const float ht = meters_at(src_col, src_row) * vert_exag_;
      const size_t o = static_cast<size_t>(out) * 3u;
      xyzp[o + 0] = dem_lon_to_x(lon);
      xyzp[o + 1] = ht;
      xyzp[o + 2] = static_cast<float>(lat);
      if (uvp) {
        uvp[static_cast<size_t>(out) * 2u + 0] =
            static_cast<float>(src_col) / u_den;
        uvp[static_cast<size_t>(out) * 2u + 1] =
            static_cast<float>(src_row) / v_den;
      }
      ++out;
    }
  };
  for_each_bake_row(mc, mr, fill_row);
  indices->reserve(static_cast<size_t>((mc - 1) * (mr - 1) * 6));
  for (int row = 0; row < mr - 1; ++row) {
    for (int col = 0; col < mc - 1; ++col) {
      const int i00 = vert_of[static_cast<size_t>(row * mc + col)];
      const int i10 = vert_of[static_cast<size_t>(row * mc + col + 1)];
      const int i01 = vert_of[static_cast<size_t>((row + 1) * mc + col)];
      const int i11 = vert_of[static_cast<size_t>((row + 1) * mc + col + 1)];
      if (i00 < 0 || i10 < 0 || i01 < 0 || i11 < 0) {
        continue;
      }
      indices->push_back(static_cast<uint32_t>(i00));
      indices->push_back(static_cast<uint32_t>(i11));
      indices->push_back(static_cast<uint32_t>(i10));
      indices->push_back(static_cast<uint32_t>(i00));
      indices->push_back(static_cast<uint32_t>(i01));
      indices->push_back(static_cast<uint32_t>(i11));
    }
  }
  return !indices->empty();
}

bool DemRaster::bake_hypsometric_rgba(int max_edge, std::vector<uint8_t>* rgba,
                                      int* out_w, int* out_h) const {
  if (!rgba || empty()) {
    return false;
  }
  base::ElapsedTimer hypso_timer;
  const bool skip_cache = bake_skip_result_cache();
  if (!skip_cache && !source_path_.empty() &&
      dem_hypso_cache_try_get(source_path_.c_str(), max_edge, rgba, out_w,
                              out_h)) {
    note_dem_phase_hypso(
        static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
        /*cache_hit=*/true);
    return !rgba->empty();
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols_ > max_edge) {
      step_x = cols_ / max_edge;
    }
    if (rows_ > max_edge) {
      step_y = rows_ / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols_ + step_x - 1) / step_x;
  const int h = (rows_ + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  const BakeBackend backend = resolved_bake_backend();
  const bool allow_cuda = backend != BakeBackend::kCpu;
  const bool require_cuda = backend == BakeBackend::kCuda;
  const uint8_t* landp = land_.empty() ? nullptr : land_.data();
  if (allow_cuda &&
      try_bake_hypso_thrust(heights_.data(), landp, cols_, rows_, step_x,
                            step_y, w, h, rgba->data())) {
    if (out_w) {
      *out_w = w;
    }
    if (out_h) {
      *out_h = h;
    }
    if (!skip_cache && !source_path_.empty() && !rgba->empty()) {
      dem_hypso_cache_put(source_path_.c_str(), max_edge, *rgba, w, h);
    }
    note_dem_phase_hypso(
        static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
        /*cache_hit=*/false);
    return true;
  }
  if (require_cuda) {
    return false;
  }
  uint8_t* pixels = rgba->data();
  auto fill_row = [&](int row) {
    const int src_row = (std::min)(rows_ - 1, row * step_y);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols_ - 1, col * step_x);
      float r = 0.05f;
      float g = 0.08f;
      float b = 0.14f;
      bool land = true;
      if (!land_.empty()) {
        land = land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
      }
      if (land) {
        float m = meters_at(src_col, src_row);
        if (m <= 1.f) {
          m = 80.f;
        }
        const int c1 = (std::min)(cols_ - 1, src_col + step_x);
        const int r1 = (std::min)(rows_ - 1, src_row + step_y);
        const float m0 = meters_at(src_col, src_row);
        const float dh = std::fabs(meters_at(c1, src_row) - m0);
        const float dv = std::fabs(meters_at(src_col, r1) - m0);
        const float slope =
            clampf(std::sqrt(dh * dh + dv * dv) / 900.f, 0.f, 1.f);
        terrain_material_rgb(m, slope, &r, &g, &b);
      }
      const size_t i =
          (static_cast<size_t>(row) * static_cast<size_t>(w) +
           static_cast<size_t>(col)) *
          4u;
      pixels[i + 0] = detail::bake_pack_u8(r);
      pixels[i + 1] = detail::bake_pack_u8(g);
      pixels[i + 2] = detail::bake_pack_u8(b);
      pixels[i + 3] = 255;
    }
  };
  for_each_bake_row(w, h, fill_row);
  if (!land_.empty() && w > 2 && h > 2) {
    auto land_at = [&](int c, int r) -> bool {
      const int src_col = (std::min)(cols_ - 1, c * step_x);
      const int src_row = (std::min)(rows_ - 1, r * step_y);
      return land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
    };
    std::vector<uint8_t> alt(rgba->size());
    uint8_t* a = rgba->data();
    uint8_t* b = alt.data();
    for (int pass = 0; pass < 3; ++pass) {
      const uint8_t* srcp = a;
      uint8_t* dstp = b;
      auto dilate_row = [&](int row) {
        for (int col = 0; col < w; ++col) {
          const size_t dst =
              (static_cast<size_t>(row) * static_cast<size_t>(w) +
               static_cast<size_t>(col)) *
              4u;
          dstp[dst + 0] = srcp[dst + 0];
          dstp[dst + 1] = srcp[dst + 1];
          dstp[dst + 2] = srcp[dst + 2];
          dstp[dst + 3] = srcp[dst + 3];
          if (land_at(col, row)) {
            continue;
          }
          const int nbs[4][2] = {{col - 1, row},
                                 {col + 1, row},
                                 {col, row - 1},
                                 {col, row + 1}};
          for (const auto& nb : nbs) {
            const int nc = nb[0];
            const int nr = nb[1];
            if (nc < 0 || nr < 0 || nc >= w || nr >= h) {
              continue;
            }
            const size_t src =
                (static_cast<size_t>(nr) * static_cast<size_t>(w) +
                 static_cast<size_t>(nc)) *
                4u;
            if (srcp[src + 0] < 40 && srcp[src + 1] < 55 &&
                srcp[src + 2] < 80 && !land_at(nc, nr)) {
              continue;
            }
            dstp[dst + 0] = srcp[src + 0];
            dstp[dst + 1] = srcp[src + 1];
            dstp[dst + 2] = srcp[src + 2];
            dstp[dst + 3] = 255;
            break;
          }
        }
      };
      for_each_bake_row(w, h, dilate_row);
      uint8_t* tmp = a;
      a = b;
      b = tmp;
    }
    if (a != rgba->data()) {
      rgba->swap(alt);
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (!skip_cache && !source_path_.empty() && !rgba->empty()) {
    dem_hypso_cache_put(source_path_.c_str(), max_edge, *rgba, w, h);
  }
  note_dem_phase_hypso(
      static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
      /*cache_hit=*/false);
  return true;
}

bool DemRaster::bake_elevation_overlay_rgba(int max_edge, bool surface,
                                            bool curves,
                                            std::vector<uint8_t>* rgba,
                                            int* out_w, int* out_h) const {
  if (!rgba || empty() || (!surface && !curves)) {
    return false;
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols_ > max_edge) {
      step_x = cols_ / max_edge;
    }
    if (rows_ > max_edge) {
      step_y = rows_ / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols_ + step_x - 1) / step_x;
  const int h = (rows_ + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }
  std::vector<float> grid(static_cast<size_t>(w) * static_cast<size_t>(h), 0.f);
  float* gp = grid.data();
  auto lod_row = [&](int row) {
    const int src_row = (std::min)(rows_ - 1, row * step_y);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols_ - 1, col * step_x);
      float m = meters_at(src_col, src_row);
      if (!land_.empty() &&
          land_[static_cast<size_t>(index_at(src_col, src_row))] == 0) {
        m = 0.f;
      }
      gp[static_cast<size_t>(row) * static_cast<size_t>(w) +
         static_cast<size_t>(col)] = m;
    }
  };
  for_each_bake_row(w, h, lod_row);
  if (!vista::bake_elevation_overlay_rgba(grid.data(), w, h, surface, curves,
                                          0.f, rgba)) {
    return false;
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  return true;
}

void set_sample_dem_path_override(const char* path) {
  if (!path || !path[0]) {
    dem_path_override_store().clear();
    return;
  }
  dem_path_override_store().assign(path);
}

std::string sample_dem_path_override() {
  return dem_path_override_store();
}

namespace {

std::string module_dir_for_samples() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  std::string dir;
  if (n > 0 && n < MAX_PATH) {
    dir.assign(module, module + n);
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
      dir.resize(slash + 1);
    }
  }
  return dir;
}

std::string first_existing_rel(const std::string& dir, const char* const* rel,
                               size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const std::string cand = join_dir(dir, rel[i]);
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
}

}  // namespace

std::string find_sample_dem_path() {
  if (!dem_path_override_store().empty()) {
    const DWORD attr =
        GetFileAttributesA(dem_path_override_store().c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return dem_path_override_store();
    }
  }
  // china_dem first: already cutlined. Preferring global_dem here makes
  // leftover China seed remask with trim_dem_mask_rings(48) and punches
  // Henan/plains holes between prefecture blobs.
  static const char* kChinaDemRel[] = {
      "..\\data\\china_dem.tif",
      "..\\data\\china_dem.tiff",
      "data\\china_dem.tif",
      "data\\china_dem.tiff",
      "china_dem.tif",
      "china_dem.tiff",
      "testing\\data\\china\\china_dem.tif",
      "testing\\data\\china\\china_dem.tiff",
      "..\\testing\\data\\china\\china_dem.tif",
      "..\\..\\testing\\data\\china\\china_dem.tif",
  };
  return first_existing_rel(module_dir_for_samples(), kChinaDemRel,
                            std::size(kChinaDemRel));
}

std::string find_sample_global_dem_path() {
  if (!dem_path_override_store().empty()) {
    const DWORD attr =
        GetFileAttributesA(dem_path_override_store().c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return dem_path_override_store();
    }
  }
  static const char* kGlobalDemRel[] = {
      "..\\data\\global_dem.tif",
      "..\\data\\global_dem.tiff",
      "..\\plugins\\world3d\\data\\global_dem.tif",
      "data\\global_dem.tif",
      "data\\global_dem.tiff",
  };
  const std::string global =
      first_existing_rel(module_dir_for_samples(), kGlobalDemRel,
                         std::size(kGlobalDemRel));
  if (!global.empty()) {
    return global;
  }
  return find_sample_dem_path();
}

std::string find_sample_imagery_path() {
  // China orthophoto first — do not steal China drape UV with global equirect.
  static const char* kChinaImageryRel[] = {
      "..\\data\\china_rs.tif",
      "..\\data\\china_imagery.tif",
      "china_rs.tif",
      "china_imagery.tif",
      "china_rs.tiff",
      "china_rs.png",
      "testing\\data\\china_rs.tif",
      "testing\\data\\china_imagery.tif",
      "testing\\data\\china_rs.png",
      "..\\testing\\data\\china_rs.tif",
      "..\\..\\testing\\data\\china_rs.tif",
  };
  return first_existing_rel(module_dir_for_samples(), kChinaImageryRel,
                            std::size(kChinaImageryRel));
}

std::string find_sample_global_imagery_path() {
  // Product GDAL is GTiff-only — prefer .tif over PNG/JPEG.
  static const char* kGlobalImageryRel[] = {
      "..\\data\\global_terrain.tif",
      "..\\data\\global_imagery.tif",
      "..\\plugins\\world3d\\data\\global_terrain.tif",
      "..\\data\\global_terrain.png",
      "..\\data\\blue_marble.png",
      "..\\plugins\\world3d\\data\\global_terrain.png",
      "..\\data\\global_terrain.jpg",
      "..\\data\\global_terrain.jpeg",
      "..\\data\\blue_marble.jpg",
      "..\\plugins\\world3d\\data\\global_terrain.jpg",
  };
  const std::string global =
      first_existing_rel(module_dir_for_samples(), kGlobalImageryRel,
                         std::size(kGlobalImageryRel));
  if (!global.empty()) {
    return global;
  }
  return find_sample_imagery_path();
}

bool load_imagery_rgba(const char* path, std::vector<uint8_t>* rgba, int* out_w,
                       int* out_h, int max_edge) {
  if (!path || !path[0] || !rgba) {
    return false;
  }
  if (max_edge < 2) {
    max_edge = 2;
  }
  GDALAllRegister();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path, GA_ReadOnly));
  if (!ds) {
    std::fprintf(stderr, "Scene3d imagery: GDALOpen failed for %s\n", path);
    return false;
  }
  const int n_x = ds->GetRasterXSize();
  const int n_y = ds->GetRasterYSize();
  const int bands = ds->GetRasterCount();
  if (n_x < 2 || n_y < 2 || bands < 1) {
    GDALClose(ds);
    std::fprintf(stderr, "Scene3d imagery: bad size/bands for %s\n", path);
    return false;
  }
  // Cap drape / globe equirect so FlyCube upload stays modest. Globe callers
  // pass 2048; default 1024 avoids multi-hundred-MB china_rs uploads.
  int out_cols = n_x;
  int out_rows = n_y;
  if (out_cols > max_edge || out_rows > max_edge) {
    const double sx = static_cast<double>(max_edge) / out_cols;
    const double sy = static_cast<double>(max_edge) / out_rows;
    const double s = (std::min)(sx, sy);
    out_cols = (std::max)(2, static_cast<int>(std::lround(n_x * s)));
    out_rows = (std::max)(2, static_cast<int>(std::lround(n_y * s)));
  }
  rgba->assign(static_cast<size_t>(out_cols) * static_cast<size_t>(out_rows) *
                   4u,
               255);
  std::vector<uint8_t> plane(
      static_cast<size_t>(out_cols) * static_cast<size_t>(out_rows), 0);
  auto read_band = [&](int band_1based, int channel) -> bool {
    GDALRasterBand* band = ds->GetRasterBand(band_1based);
    if (!band) {
      return false;
    }
    const CPLErr err =
        band->RasterIO(GF_Read, 0, 0, n_x, n_y, plane.data(), out_cols,
                       out_rows, GDT_Byte, 0, 0);
    if (err != CE_None) {
      return false;
    }
    for (size_t i = 0; i < plane.size(); ++i) {
      (*rgba)[i * 4u + static_cast<size_t>(channel)] = plane[i];
    }
    return true;
  };
  bool ok = false;
  if (bands >= 3) {
    ok = read_band(1, 0) && read_band(2, 1) && read_band(3, 2);
    if (ok && bands >= 4) {
      (void)read_band(4, 3);
    }
  } else {
    // Single-band greyscale → RGB.
    ok = read_band(1, 0);
    if (ok) {
      for (size_t i = 0; i < plane.size(); ++i) {
        (*rgba)[i * 4u + 1] = plane[i];
        (*rgba)[i * 4u + 2] = plane[i];
      }
    }
  }
  GDALClose(ds);
  if (!ok) {
    rgba->clear();
    std::fprintf(stderr, "Scene3d imagery: RasterIO failed for %s\n", path);
    return false;
  }
  if (out_w) {
    *out_w = out_cols;
  }
  if (out_h) {
    *out_h = out_rows;
  }
  return true;
}


void dem_seed_lonlat_box(double in_minx, double in_miny, double in_maxx,
                          double in_maxy, double* out_minx, double* out_miny,
                          double* out_maxx, double* out_maxy) {
  constexpr double kChinaMinX = 73.0;
  constexpr double kChinaMinY = 18.0;
  constexpr double kChinaMaxX = 135.0;
  constexpr double kChinaMaxY = 54.0;
  const bool nonempty = in_maxx > in_minx && in_maxy > in_miny;
  const bool looks_china = nonempty && in_minx >= 60.0 && in_maxx <= 145.0 &&
                           in_miny >= 3.0 && in_maxy <= 60.0;
  double minx = in_minx;
  double miny = in_miny;
  double maxx = in_maxx;
  double maxy = in_maxy;
  if (sample_dem_path_override().empty()) {
    if (!looks_china) {
      minx = kChinaMinX;
      miny = kChinaMinY;
      maxx = kChinaMaxX;
      maxy = kChinaMaxY;
    }
  } else if (!nonempty) {
    minx = kChinaMinX;
    miny = kChinaMinY;
    maxx = kChinaMaxX;
    maxy = kChinaMaxY;
  }
  if (out_minx) {
    *out_minx = minx;
  }
  if (out_miny) {
    *out_miny = miny;
  }
  if (out_maxx) {
    *out_maxx = maxx;
  }
  if (out_maxy) {
    *out_maxy = maxy;
  }
}

int dem_seed_cache_key(float camera_distance) {
  const int next_lod = DemRaster::lod_max_edge(camera_distance);
  const int grid_key = camera_distance < 2.4f ? 2 : 1;
  return next_lod * 10 + grid_key;
}

bool DemRaster::sample_globe_surface(double minx, double miny, double maxx,
                                     double maxy, bool global_grid,
                                     const char* imagery_path,
                                     std::vector<float>* heights, int* cols,
                                     int* rows, std::vector<uint8_t>* rgba,
                                     int* tex_w, int* tex_h,
                                     bool* imagery_loaded) const {
  if (imagery_loaded) {
    *imagery_loaded = false;
  }
  if (!heights || !cols || !rows || !rgba || !tex_w || !tex_h) {
    return false;
  }
  if (!(maxx > minx) || !(maxy > miny)) {
    return false;
  }
  const int k_cols = global_grid ? 1024 : 1024;
  const int k_rows = global_grid ? 512 : 640;
  heights->assign(static_cast<size_t>(k_cols) * static_cast<size_t>(k_rows),
                  0.f);
  for (int r = 0; r < k_rows; ++r) {
    const double lat =
        maxy - (static_cast<double>(r) + 0.5) / k_rows * (maxy - miny);
    for (int c = 0; c < k_cols; ++c) {
      const double lon =
          minx + (static_cast<double>(c) + 0.5) / k_cols * (maxx - minx);
      (*heights)[static_cast<size_t>(r * k_cols + c)] = sample_meters(lon, lat);
    }
  }
  *cols = k_cols;
  *rows = k_rows;
  rgba->clear();
  *tex_w = 0;
  *tex_h = 0;
  bool have_imagery = false;
  if (imagery_path && imagery_path[0] != '\0') {
    have_imagery = load_imagery_rgba(imagery_path, rgba, tex_w, tex_h,
                                     global_grid ? 2048 : 1024) &&
                   *tex_w > 0 && *tex_h > 0;
    if (!have_imagery) {
      rgba->clear();
      *tex_w = 0;
      *tex_h = 0;
    }
  }
  if (!have_imagery) {
    (void)bake_hypsometric_rgba(global_grid ? 1024 : 1024, rgba, tex_w, tex_h);
  }
  if (imagery_loaded) {
    *imagery_loaded = have_imagery;
  }
  return true;
}


}  // namespace vista

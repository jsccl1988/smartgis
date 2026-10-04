// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/terrain/dem/dem_raster.h"

#include "base/time/elapsed_timer.h"
#include "vista/world/terrain/dem/dem_bake_cache.h"
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
  return (std::max)(lo, (std::min)(hi, v));
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
  if (!r || !g || !b) {
    return;
  }
  if (meters <= 1.f) {
    // Deep coastal water (not cyan wash).
    *r = 0.10f;
    *g = 0.18f;
    *b = 0.28f;
    return;
  }
  // Four-stop atlas: sage plains → olive hills → ochre highland → cool rock.
  // Keep g-dominant lowlands for Scene3d landish BMP gates.
  const float t01 = (std::max)(0.f, (std::min)(1.f, meters / 5500.f));
  if (t01 < 0.28f) {
    const float u = t01 / 0.28f;
    *r = (58.f + 42.f * u) / 255.f;
    *g = (118.f + 36.f * u) / 255.f;
    *b = (72.f + 18.f * (1.f - u)) / 255.f;
  } else if (t01 < 0.52f) {
    const float u = (t01 - 0.28f) / 0.24f;
    *r = (100.f + 48.f * u) / 255.f;
    *g = (154.f - 18.f * u) / 255.f;
    *b = (68.f + 12.f * u) / 255.f;
  } else if (t01 < 0.78f) {
    const float u = (t01 - 0.52f) / 0.26f;
    *r = (148.f + 36.f * u) / 255.f;
    *g = (136.f - 8.f * u) / 255.f;
    *b = (80.f + 20.f * u) / 255.f;
  } else {
    const float u = (t01 - 0.78f) / 0.22f;
    *r = (164.f + 28.f * u) / 255.f;
    *g = (148.f + 22.f * u) / 255.f;
    *b = (118.f + 28.f * u) / 255.f;
  }
}

// Albedo for the lit PBR pass: elevation ramp, then rock on steep faces
// and a cool snow cap on high flats.
void terrain_material_rgb(float meters, float slope01, float* r, float* g,
                          float* b) {
  hypsometric_rgb(meters, r, g, b);
  const float rock = clampf(slope01, 0.f, 1.f);
  // Cool slate rock — less muddy brown on faceted DEM triangles.
  const float w = 0.55f * rock;
  *r = *r * (1.f - w) + 0.42f * w;
  *g = *g * (1.f - w) + 0.40f * w;
  *b = *b * (1.f - w) + 0.38f * w;
  if (meters > 4200.f && rock < 0.45f) {
    const float u =
        clampf((meters - 4200.f) / 2200.f, 0.f, 1.f) * (1.f - rock);
    // Soft cool snow — keep below ~0.58 so D3D lit response stays ochre/rock.
    *r = *r * (1.f - u) + 0.55f * u;
    *g = *g * (1.f - u) + 0.58f * u;
    *b = *b * (1.f - u) + 0.62f * u;
  }
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
  for (const LonLatRing& ring : rings) {
    ring.prepare_bbox();
  }
  land_.assign(static_cast<size_t>(cols_ * rows_), 0);
  const double dx = (maxx_ - minx_) / (std::max)(1, cols_ - 1);
  const double dy = (maxy_ - miny_) / (std::max)(1, rows_ - 1);
  for (int row = 0; row < rows_; ++row) {
    const double lat = maxy_ - row * dy;
    for (int col = 0; col < cols_; ++col) {
      const double lon = minx_ + col * dx;
      if (any_ring_contains(lon, lat, rings)) {
        land_[static_cast<size_t>(index_at(col, row))] = 1;
      } else {
        heights_[static_cast<size_t>(index_at(col, row))] = 0.f;
      }
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
  for (int row = 0; row < mr; ++row) {
    const int src_row =
        (std::min)(row1, row0 + (std::min)(win_rows - 1, row * step_y));
    const double lat = maxy_ - src_row * dy;
    for (int col = 0; col < mc; ++col) {
      const int src_col =
          (std::min)(col1, col0 + (std::min)(win_cols - 1, col * step_x));
      if (!is_land(src_col, src_row)) {
        continue;
      }
      vert_of[static_cast<size_t>(row * mc + col)] =
          static_cast<int>(xyz->size() / 3);
      const double lon = minx_ + src_col * dx;
      const float h = meters_at(src_col, src_row) * vert_exag_;
      xyz->push_back(dem_lon_to_x(lon));
      xyz->push_back(h);
      xyz->push_back(static_cast<float>(lat));
      if (uvs) {
        uvs->push_back(static_cast<float>(src_col) / u_den);
        uvs->push_back(static_cast<float>(src_row) / v_den);
      }
    }
  }
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
  if (!source_path_.empty() &&
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
  for (int row = 0; row < h; ++row) {
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
        // Coastal DEM often stores ≤1 m; pale ocean blue there made the
        // mainland silhouette read as a flat cyan slab under FlyCube.
        float m = meters_at(src_col, src_row);
        if (m <= 1.f) {
          m = 80.f;
        }
        const int c1 = (std::min)(cols_ - 1, src_col + step_x);
        const int r1 = (std::min)(rows_ - 1, src_row + step_y);
        const float dh = std::fabs(meters_at(c1, src_row) - meters_at(src_col, src_row));
        const float dv = std::fabs(meters_at(src_col, r1) - meters_at(src_col, src_row));
        const float slope =
            clampf(std::sqrt(dh * dh + dv * dv) / 900.f, 0.f, 1.f);
        terrain_material_rgb(m, slope, &r, &g, &b);
      }
      // Non-land stays deep navy (not 0.18/0.36/0.52 cyan) so mid-frame
      // visual gates do not treat the whole DEM AABB as flat cyan.
      const size_t i =
          (static_cast<size_t>(row) * static_cast<size_t>(w) +
           static_cast<size_t>(col)) *
          4u;
      (*rgba)[i + 0] = static_cast<uint8_t>(clampf(r, 0.f, 1.f) * 255.f + 0.5f);
      (*rgba)[i + 1] = static_cast<uint8_t>(clampf(g, 0.f, 1.f) * 255.f + 0.5f);
      (*rgba)[i + 2] = static_cast<uint8_t>(clampf(b, 0.f, 1.f) * 255.f + 0.5f);
      (*rgba)[i + 3] = 255;
    }
  }
  // Dilate land albedo two texels into coastal ocean. Land-only mesh UVs can
  // still sample the ocean side of the coast; navy bleed painted China black
  // under FlyCube (atmosphere.full landish gate) and left shoreline seams on
  // D3D china showcase.
  if (!land_.empty() && w > 2 && h > 2) {
    auto land_at = [&](int c, int r) -> bool {
      const int src_col = (std::min)(cols_ - 1, c * step_x);
      const int src_row = (std::min)(rows_ - 1, r * step_y);
      return land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
    };
    for (int pass = 0; pass < 3; ++pass) {
      std::vector<uint8_t> dilated = *rgba;
      for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
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
            // Copy from any non-navy neighbor (land or already dilated).
            if ((*rgba)[src + 0] < 40 && (*rgba)[src + 1] < 55 &&
                (*rgba)[src + 2] < 80 && !land_at(nc, nr)) {
              continue;
            }
            const size_t dst =
                (static_cast<size_t>(row) * static_cast<size_t>(w) +
                 static_cast<size_t>(col)) *
                4u;
            dilated[dst + 0] = (*rgba)[src + 0];
            dilated[dst + 1] = (*rgba)[src + 1];
            dilated[dst + 2] = (*rgba)[src + 2];
            dilated[dst + 3] = 255;
            break;
          }
        }
      }
      *rgba = std::move(dilated);
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (!source_path_.empty() && !rgba->empty()) {
    dem_hypso_cache_put(source_path_.c_str(), max_edge, *rgba, w, h);
  }
  note_dem_phase_hypso(
      static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
      /*cache_hit=*/false);
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
      "testing\\data\\china_dem.tif",
      "testing\\data\\china_dem.tiff",
      "..\\testing\\data\\china_dem.tif",
      "..\\..\\testing\\data\\china_dem.tif",
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
                       int* out_h) {
  if (!path || !path[0] || !rgba) {
    return false;
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
  // Cap drape / globe equirect so FlyCube upload stays modest (allows
  // 1024x512 global_terrain.png without multi-hundred-MB china_rs).
  constexpr int kMaxEdge = 1024;
  int out_cols = n_x;
  int out_rows = n_y;
  if (out_cols > kMaxEdge || out_rows > kMaxEdge) {
    const double sx = static_cast<double>(kMaxEdge) / out_cols;
    const double sy = static_cast<double>(kMaxEdge) / out_rows;
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

Node* seed_dem_raster_into_world(World* world, const DemRaster& dem,
                                 const char* name, int max_edge) {
  if (!world || dem.empty()) {
    return nullptr;
  }
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  dem.envelope(&min_x, &min_y, &max_x, &max_y);
  const double min_z =
      static_cast<double>(dem.min_meters() * dem.vertical_exaggeration());
  const double max_z =
      static_cast<double>(dem.max_meters() * dem.vertical_exaggeration());
  const char* node_name = (name && name[0]) ? name : "dem";
  Node* node =
      world->attach_terrain(node_name, min_x, min_y, min_z, max_x, max_y, max_z);
  if (!node) {
    return nullptr;
  }
  // Fallback when callers pass 0/1: match national-frame LOD density for
  // china_dem 1536×960 (not the old soft 96-edge bake).
  const int edge = max_edge > 1 ? max_edge : 512;
  std::vector<float> xyz;
  std::vector<uint32_t> idx;
  std::vector<float> uvs;
  {
    base::ElapsedTimer tess_timer;
    double dminx = 0;
    double dminy = 0;
    double dmaxx = 0;
    double dmaxy = 0;
    dem.envelope(&dminx, &dminy, &dmaxx, &dmaxy);
    const char* sp =
        dem.source_path().empty() ? nullptr : dem.source_path().c_str();
    bool have = sp && dem_mesh_cache_try_get(sp, edge, dminx, dminy, dmaxx,
                                            dmaxy, /*windowed=*/false,
                                            /*apply_land_mask=*/true, &xyz,
                                            &idx, &uvs);
    if (!have) {
      have = dem.build_mesh(edge, &xyz, &idx, &uvs) && !xyz.empty() &&
             !idx.empty();
      if (have && sp) {
        dem_mesh_cache_put(sp, edge, dminx, dminy, dmaxx, dmaxy,
                           /*windowed=*/false, /*apply_land_mask=*/true, xyz,
                           idx, uvs);
      }
    }
    note_dem_phase_tess(
        static_cast<int64_t>(tess_timer.elapsed_milliseconds() + 0.5));
    if (!have) {
      return node;
    }
  }
  world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                          idx.size());
  if (!uvs.empty()) {
    world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
  }

  std::vector<uint8_t> rgba;
  int tw = 0;
  int th = 0;
  // Hypsometric bake matches land-only mesh UVs. china_rs drape often leaves a
  // flat cyan sticker when imagery resolution and mesh LOD diverge.
  const bool have_tex = dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th);
  if (have_tex && tw > 0 && th > 0) {
    world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                               static_cast<uint32_t>(tw),
                               static_cast<uint32_t>(th));
  }
  return world->find(node->id);
}

Node* seed_dem_raster_lod_into_world(World* world, const DemRaster& dem,
                                     const char* name,
                                     float camera_distance) {
  const int edge = DemRaster::lod_max_edge(camera_distance);
  return seed_dem_raster_into_world(world, dem, name, edge);
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
  const int k_cols = global_grid ? 512 : 768;
  const int k_rows = global_grid ? 256 : 480;
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
    have_imagery = load_imagery_rgba(imagery_path, rgba, tex_w, tex_h) &&
                   *tex_w > 0 && *tex_h > 0;
    if (!have_imagery) {
      rgba->clear();
      *tex_w = 0;
      *tex_h = 0;
    }
  }
  if (!have_imagery) {
    (void)bake_hypsometric_rgba(global_grid ? 384 : 512, rgba, tex_w, tex_h);
  }
  if (imagery_loaded) {
    *imagery_loaded = have_imagery;
  }
  return true;
}

size_t seed_dem_view_tiles_into_world(World* world, const DemRaster& dem,
                                      double view_minx, double view_miny,
                                      double view_maxx, double view_maxy,
                                      float camera_distance,
                                      int max_total_vertices,
                                      const char* name_prefix) {
  if (!world || dem.empty()) {
    return 0;
  }
  double dem_minx = 0;
  double dem_miny = 0;
  double dem_maxx = 0;
  double dem_maxy = 0;
  dem.envelope(&dem_minx, &dem_miny, &dem_maxx, &dem_maxy);
  const double minx = (std::max)(view_minx, dem_minx);
  const double miny = (std::max)(view_miny, dem_miny);
  const double maxx = (std::min)(view_maxx, dem_maxx);
  const double maxy = (std::min)(view_maxy, dem_maxy);
  if (!(maxx > minx) || !(maxy > miny)) {
    return 0;
  }
  // Far → 1 tile; mid → 2×2; near → 4×4. Zoom-in replaces coarse with finer.
  // Small framed extents (coastal showcase pads, city AOI) stay one tile —
  // multi-tile left/right hypsometric seams and chewed land-mask edges.
  const double span =
      (std::max)(maxx - minx, maxy - miny);
  int grid = 1;
  if (span > 2.0) {
    if (camera_distance < 1.2f) {
      grid = 4;
    } else if (camera_distance < 2.4f) {
      grid = 2;
    }
  }
  // Regional windows: skip land mask so the mesh keeps a rectangular skirt
  // instead of a chewed coastline silhouette against the clear color.
  const bool apply_land_mask = span > 2.0;
  int edge = DemRaster::lod_max_edge(camera_distance);
  // Cap per-tile edge so grid*edge^2 stays under the vertex budget.
  // Default budget fits one ~512×320 china_dem tile (≈163k verts) with headroom.
  const int budget =
      max_total_vertices > 64 ? max_total_vertices : 196608;
  const int max_edge_per_tile =
      (std::max)(8, static_cast<int>(
                        std::sqrt(static_cast<double>(budget) /
                                  static_cast<double>(grid * grid))));
  edge = (std::min)(edge, max_edge_per_tile);

  const char* prefix =
      (name_prefix && name_prefix[0]) ? name_prefix : "dem_tile";
  const double tile_w = (maxx - minx) / static_cast<double>(grid);
  const double tile_h = (maxy - miny) / static_cast<double>(grid);
  size_t attached = 0;
  size_t total_verts = 0;
  for (int ty = 0; ty < grid; ++ty) {
    for (int tx = 0; tx < grid; ++tx) {
      const double tminx = minx + tx * tile_w;
      const double tmaxx = minx + (tx + 1) * tile_w;
      const double tminy = miny + ty * tile_h;
      const double tmaxy = miny + (ty + 1) * tile_h;
      std::vector<float> xyz;
      std::vector<uint32_t> idx;
      std::vector<float> uvs;
      {
        base::ElapsedTimer tess_timer;
        const char* sp =
            dem.source_path().empty() ? nullptr : dem.source_path().c_str();
        bool have =
            sp && dem_mesh_cache_try_get(sp, edge, tminx, tminy, tmaxx, tmaxy,
                                         /*windowed=*/true, apply_land_mask,
                                         &xyz, &idx, &uvs);
        if (!have) {
          have = dem.build_mesh_window(tminx, tminy, tmaxx, tmaxy, edge, &xyz,
                                       &idx, &uvs, apply_land_mask) &&
                 !xyz.empty() && !idx.empty();
          if (have && sp) {
            dem_mesh_cache_put(sp, edge, tminx, tminy, tmaxx, tmaxy,
                               /*windowed=*/true, apply_land_mask, xyz, idx,
                               uvs);
          }
        }
        note_dem_phase_tess(
            static_cast<int64_t>(tess_timer.elapsed_milliseconds() + 0.5));
        if (!have) {
          continue;
        }
      }
      const size_t verts = xyz.size() / 3;
      if (total_verts + verts > static_cast<size_t>(budget) && attached > 0) {
        break;
      }
      float mn_x = xyz[0];
      float mn_y = xyz[1];
      float mn_z = xyz[2];
      float mx_x = mn_x;
      float mx_y = mn_y;
      float mx_z = mn_z;
      for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
        mn_x = (std::min)(mn_x, xyz[i]);
        mn_y = (std::min)(mn_y, xyz[i + 1]);
        mn_z = (std::min)(mn_z, xyz[i + 2]);
        mx_x = (std::max)(mx_x, xyz[i]);
        mx_y = (std::max)(mx_y, xyz[i + 1]);
        mx_z = (std::max)(mx_z, xyz[i + 2]);
      }
      char name_buf[64];
      std::snprintf(name_buf, sizeof(name_buf), "%s_%d_%d", prefix, tx, ty);
      Node* node = world->attach_terrain(name_buf, static_cast<double>(mn_x),
                                         static_cast<double>(mn_y),
                                         static_cast<double>(mn_z),
                                         static_cast<double>(mx_x),
                                         static_cast<double>(mx_y),
                                         static_cast<double>(mx_z));
      if (!node) {
        continue;
      }
      world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                              idx.size());
      if (!uvs.empty()) {
        world->set_terrain_uvs(node->id, uvs.data(), uvs.size());
      }
      std::vector<uint8_t> rgba;
      int tw = 0;
      int th = 0;
      if (dem.bake_hypsometric_rgba(edge, &rgba, &tw, &th) && tw > 0 &&
          th > 0) {
        world->set_terrain_texture(node->id, rgba.data(), rgba.size(),
                                   static_cast<uint32_t>(tw),
                                   static_cast<uint32_t>(th));
      }
      total_verts += verts;
      ++attached;
    }
  }
  return attached;
}

Node* seed_china_dem_into_world(World* world, const LonLatRing* rings,
                                size_t ring_count, const char* name,
                                int max_edge) {
  if (!world) {
    return nullptr;
  }
  DemRaster dem;
  const std::string path = find_sample_dem_path();
  if (path.empty() || !dem.load_gdal_raster(path.c_str()) || dem.empty()) {
    // Real-data policy: no synthetic China DEM stand-in.
    return nullptr;
  }
  // Real china_dem* already encodes land/ocean. Remasking with prefecture
  // rings can punch holes (mainland-contains caution). Match leftover
  // seed_stereo_underlay: skip mask_outside_rings when the loaded path is
  // china_dem. Other rasters may still mask with rings.
  const bool skip_cutline = path.find("china_dem") != std::string::npos;
  if (!skip_cutline && rings && ring_count > 0) {
    std::vector<LonLatRing> clip(rings, rings + ring_count);
    dem.mask_outside_rings(clip);
  }
  return seed_dem_raster_into_world(world, dem, name, max_edge);
}

}  // namespace vista

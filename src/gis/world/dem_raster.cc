// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/world/dem_raster.h"

#include "gdal_priv.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace gis {
namespace {

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

bool DemRaster::load_gdal_raster(const char* path) {
  heights_.clear();
  land_.clear();
  cols_ = 0;
  rows_ = 0;
  if (!path || !path[0]) {
    return false;
  }
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
  downsample_to_max_edge(384);
  recompute_range();
  fit_vertical_exaggeration();
  return !empty();
}

void DemRaster::fill_synthetic_china() {
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
  land_.clear();
  cols_ = new_cols;
  rows_ = new_rows;
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

bool DemRaster::build_mesh(int max_edge, std::vector<float>* xyz,
                           std::vector<uint32_t>* indices) const {
  if (!xyz || !indices || empty()) {
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
  const int mc = (cols_ + step_x - 1) / step_x;
  const int mr = (rows_ + step_y - 1) / step_y;
  if (mc < 2 || mr < 2) {
    return false;
  }
  xyz->clear();
  indices->clear();
  auto is_land = [&](int src_col, int src_row) -> bool {
    if (land_.empty()) {
      return true;
    }
    src_col = (std::max)(0, (std::min)(cols_ - 1, src_col));
    src_row = (std::max)(0, (std::min)(rows_ - 1, src_row));
    return land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
  };
  std::vector<int> vert_of(static_cast<size_t>(mc * mr), -1);
  const double dx = (maxx_ - minx_) / (std::max)(1, cols_ - 1);
  const double dy = (maxy_ - miny_) / (std::max)(1, rows_ - 1);
  for (int row = 0; row < mr; ++row) {
    const int src_row = (std::min)(rows_ - 1, row * step_y);
    const double lat = maxy_ - src_row * dy;
    for (int col = 0; col < mc; ++col) {
      const int src_col = (std::min)(cols_ - 1, col * step_x);
      if (!is_land(src_col, src_row)) {
        continue;
      }
      vert_of[static_cast<size_t>(row * mc + col)] =
          static_cast<int>(xyz->size() / 3);
      const double lon = minx_ + src_col * dx;
      const float h = meters_at(src_col, src_row) * vert_exag_;
      // X=-lon: RH lookAt looking +Z puts east on screen-right.
      xyz->push_back(dem_lon_to_x(lon));
      xyz->push_back(h);
      xyz->push_back(static_cast<float>(lat));
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
      // Reverse winding vs +lon mesh so front faces stay up after X mirror.
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

std::string find_sample_dem_path() {
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
  const char* rel[] = {
      "china_dem.tif",
      "china_dem.tiff",
      "testing\\data\\china_dem.tif",
      "testing\\data\\china_dem.tiff",
      "..\\testing\\data\\china_dem.tif",
      "..\\..\\testing\\data\\china_dem.tif",
  };
  for (const char* r : rel) {
    const std::string cand = join_dir(dir, r);
    const DWORD attr = GetFileAttributesA(cand.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      return cand;
    }
  }
  return {};
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
  const int edge = max_edge > 1 ? max_edge : 96;
  std::vector<float> xyz;
  std::vector<uint32_t> idx;
  if (!dem.build_mesh(edge, &xyz, &idx) || xyz.empty() || idx.empty()) {
    return node;
  }
  world->set_terrain_mesh(node->id, xyz.data(), xyz.size(), idx.data(),
                          idx.size());
  return world->find(node->id);
}

Node* seed_china_dem_into_world(World* world, const LonLatRing* rings,
                                size_t ring_count, const char* name,
                                int max_edge) {
  if (!world) {
    return nullptr;
  }
  DemRaster dem;
  const std::string path = find_sample_dem_path();
  const bool loaded_sample =
      !path.empty() && dem.load_gdal_raster(path.c_str());
  if (!loaded_sample) {
    dem.fill_synthetic_china();
  }
  // Real china_dem* already encodes land/ocean. Remasking with prefecture
  // rings can punch holes (mainland-contains caution). Match leftover
  // seed_stereo_underlay: skip mask_outside_rings when the loaded path is
  // china_dem. Synthetic / other rasters may still mask with rings.
  const bool skip_cutline =
      loaded_sample && path.find("china_dem") != std::string::npos;
  if (!skip_cutline && rings && ring_count > 0) {
    std::vector<LonLatRing> clip(rings, rings + ring_count);
    dem.mask_outside_rings(clip);
  }
  return seed_dem_raster_into_world(world, dem, name, max_edge);
}

}  // namespace gis

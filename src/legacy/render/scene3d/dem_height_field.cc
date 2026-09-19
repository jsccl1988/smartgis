// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/scene3d/dem_height_field.h"

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

namespace render {
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

// Coarse China physiography for demos when china_dem.tif is absent.
// Peaks track major ranges; basins stay low so draped 2D vectors sit sensibly.
float synthetic_meters(double lon, double lat) {
  float m = 80.f;
  // Tibetan Plateau base (west-central high ground).
  m += 2800.f * static_cast<float>(std::exp(
           -0.5 * ((lon - 90.0) / 14.0) * ((lon - 90.0) / 14.0) -
           0.5 * ((lat - 33.0) / 6.5) * ((lat - 33.0) / 6.5)));
  m += gauss_hill(lon, lat, 86.5, 28.0, 3.8, 1.8, 2200.f);   // Himalaya
  m += gauss_hill(lon, lat, 91.0, 30.5, 5.5, 2.8, 1600.f);   // Gangdise
  m += gauss_hill(lon, lat, 99.0, 28.5, 2.8, 2.2, 2400.f);   // Hengduan
  m += gauss_hill(lon, lat, 102.5, 27.5, 2.2, 1.8, 1800.f);  // Yunnan plateau
  m += gauss_hill(lon, lat, 85.0, 42.5, 5.5, 1.5, 2200.f);   // Tianshan
  m += gauss_hill(lon, lat, 88.0, 38.5, 6.0, 1.4, 1800.f);   // Kunlun
  m += gauss_hill(lon, lat, 100.0, 38.0, 4.5, 1.6, 1400.f);  // Qilian
  m += gauss_hill(lon, lat, 107.5, 34.0, 3.5, 1.4, 900.f);   // Qinling
  m += gauss_hill(lon, lat, 112.5, 37.5, 1.8, 2.5, 700.f);   // Taihang
  m += gauss_hill(lon, lat, 127.5, 42.5, 2.0, 1.8, 900.f);   // Changbai
  m += gauss_hill(lon, lat, 121.0, 23.8, 0.55, 1.1, 2400.f); // Taiwan
  m += gauss_hill(lon, lat, 109.8, 19.0, 0.7, 0.5, 600.f);   // Hainan
  m += gauss_hill(lon, lat, 117.0, 27.5, 2.2, 1.6, 600.f);   // Wuyi
  // Basins / plains (subtract).
  m -= gauss_hill(lon, lat, 84.0, 40.5, 4.5, 2.2, 2200.f);   // Tarim
  m -= gauss_hill(lon, lat, 87.0, 46.0, 3.5, 1.8, 1800.f);   // Junggar
  m -= gauss_hill(lon, lat, 105.5, 30.5, 2.5, 1.8, 1200.f);  // Sichuan basin
  m -= gauss_hill(lon, lat, 116.5, 33.0, 6.0, 4.5, 400.f);   // N. China plain
  m -= gauss_hill(lon, lat, 120.5, 31.5, 3.5, 2.0, 250.f);   // Yangtze delta

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

void hypsometric_rgb(float meters, float* r, float* g, float* b) {
  // Smooth gradient (no hard band edges) to avoid false contour striping.
  const float t = clampf(meters / 5500.f, 0.f, 1.f);
  if (meters <= 1.f) {
    *r = 0.18f;
    *g = 0.36f;
    *b = 0.52f;
    return;
  }
  // low green → mid olive → high taupe → snow.
  const float r0 = 0.45f + 0.22f * t;
  const float g0 = 0.58f - 0.12f * t;
  const float b0 = 0.38f + 0.08f * t;
  const float snow = clampf((meters - 3800.f) / 2200.f, 0.f, 1.f);
  *r = r0 * (1.f - snow) + 0.86f * snow;
  *g = g0 * (1.f - snow) + 0.84f * snow;
  *b = b0 * (1.f - snow) + 0.82f * snow;
}

bool boxes_overlap(const MapLabelBox& a, const MapLabelBox& b) {
  return a.left < b.right && a.right > b.left && a.top < b.bottom &&
         a.bottom > b.top;
}

bool box_in_view(const MapLabelBox& b, int view_w, int view_h) {
  if (view_w <= 0 || view_h <= 0) {
    return true;
  }
  return b.right > 4 && b.left < view_w - 4 && b.bottom > 4 &&
         b.top < view_h - 4;
}

std::string join_dir(const std::string& dir, const char* rel) {
  return dir + rel;
}

}  // namespace

bool DemHeightField::load_gdal_raster(const char* path) {
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

void DemHeightField::fill_synthetic_china() {
  // Dense enough that coast / ridges do not staircase at country view.
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

void DemHeightField::fit_vertical_exaggeration() {
  const float span =
      static_cast<float>((std::max)(maxx_ - minx_, maxy_ - miny_));
  const float peak = (std::max)(80.f, max_m_ - min_m_);
  // Milder relief so draped 2D lines stay visually on the surface.
  vert_exag_ = (span * 0.09f) / peak;
}

float DemHeightField::meters_at(int col, int row) const {
  col = (std::max)(0, (std::min)(cols_ - 1, col));
  row = (std::max)(0, (std::min)(rows_ - 1, row));
  return heights_[static_cast<size_t>(index_at(col, row))];
}

void DemHeightField::recompute_range() {
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

void DemHeightField::downsample_to_max_edge(int max_edge) {
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

void LonLatRing::prepare_bbox() const {
  has_bbox = false;
  if (empty()) {
    return;
  }
  minx = maxx = x[0];
  miny = maxy = y[0];
  for (size_t i = 1; i < x.size(); ++i) {
    minx = (std::min)(minx, x[i]);
    maxx = (std::max)(maxx, x[i]);
    miny = (std::min)(miny, y[i]);
    maxy = (std::max)(maxy, y[i]);
  }
  has_bbox = true;
}

bool LonLatRing::bbox_may_contain(double px, double py) const {
  if (!has_bbox) {
    prepare_bbox();
  }
  return has_bbox && px >= minx && px <= maxx && py >= miny && py <= maxy;
}

bool point_in_lonlat_ring(double px, double py, const LonLatRing& ring) {
  // Inline even-odd — do not copy ring.x/y into gis::LonLatRing per cell
  // (that O(cells×vertices) allocation hung 3D view open on china_city).
  if (ring.empty()) {
    return false;
  }
  const size_t n = ring.x.size();
  bool inside = false;
  size_t j = n - 1;
  for (size_t i = 0; i < n; ++i) {
    const double xi = ring.x[i];
    const double yi = ring.y[i];
    const double xj = ring.x[j];
    const double yj = ring.y[j];
    const bool hit = ((yi > py) != (yj > py)) &&
                     (px < (xj - xi) * (py - yi) / ((yj - yi) + 0.0) + xi);
    if (hit) {
      inside = !inside;
    }
    j = i;
  }
  return inside;
}

bool any_ring_contains(double px, double py,
                       const std::vector<LonLatRing>& rings) {
  for (const LonLatRing& ring : rings) {
    if (!ring.bbox_may_contain(px, py)) {
      continue;
    }
    if (point_in_lonlat_ring(px, py, ring)) {
      return true;
    }
  }
  return false;
}

void DemHeightField::mask_outside_rings(const std::vector<LonLatRing>& rings) {
  if (empty() || rings.empty()) {
    return;
  }
  // Bbox cull then inline even-odd (see point_in_lonlat_ring).
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
        // Flatten ocean so coast quads do not stretch tall shards seaward.
        heights_[static_cast<size_t>(index_at(col, row))] = 0.f;
      }
    }
  }
  recompute_range();
  fit_vertical_exaggeration();
}

float DemHeightField::sample_meters(double x, double y) const {
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

float DemHeightField::sample(double x, double y) const {
  return sample_meters(x, y) * vert_exag_;
}

void DemHeightField::sample_normal(double x, double y, float* nx, float* ny,
                                   float* nz) const {
  const float span =
      static_cast<float>((std::max)(maxx_ - minx_, maxy_ - miny_));
  const float eps = (std::max)(0.08f, span / 180.f);
  const float hx0 = sample(x - eps, y);
  const float hx1 = sample(x + eps, y);
  const float hy0 = sample(x, y - eps);
  const float hy1 = sample(x, y + eps);
  // Leftover 3D: X east, Y up, Z north.
  const float dx = 2.f * eps;
  const float dz = 2.f * eps;
  float x_c = dz * (hx0 - hx1);
  float y_c = dx * dz;
  float z_c = dx * (hy0 - hy1);
  const float len = std::sqrt(x_c * x_c + y_c * y_c + z_c * z_c);
  if (len < 1e-8f) {
    x_c = 0.f;
    y_c = 1.f;
    z_c = 0.f;
  } else {
    x_c /= len;
    y_c /= len;
    z_c /= len;
  }
  if (nx) {
    *nx = x_c;
  }
  if (ny) {
    *ny = y_c;
  }
  if (nz) {
    *nz = z_c;
  }
}

float DemHeightField::drape_lift() const {
  const float span =
      static_cast<float>((std::max)(maxx_ - minx_, maxy_ - miny_));
  // Keep vectors clearly above the DEM to avoid z-fighting stripes.
  return span * 0.0014f + 0.08f;
}

void DemHeightField::envelope(double* minx, double* miny, double* maxx,
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

bool DemHeightField::build_mesh(int max_edge, std::vector<float>* xyz,
                                std::vector<unsigned>* indices,
                                std::vector<float>* rgb,
                                std::vector<float>* nrm) const {
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
  if (rgb) {
    rgb->clear();
  }
  if (nrm) {
    nrm->clear();
  }
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
      const float meters = meters_at(src_col, src_row);
      const float h = meters * vert_exag_;
      xyz->push_back(static_cast<float>(lon));
      xyz->push_back(h);
      xyz->push_back(static_cast<float>(lat));
      if (rgb) {
        float r = 0;
        float g = 0;
        float b = 0;
        hypsometric_rgb(meters, &r, &g, &b);
        rgb->push_back(r);
        rgb->push_back(g);
        rgb->push_back(b);
      }
      if (nrm) {
        float nx = 0;
        float ny = 1;
        float nz = 0;
        sample_normal(lon, lat, &nx, &ny, &nz);
        nrm->push_back(nx);
        nrm->push_back(ny);
        nrm->push_back(nz);
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
      indices->push_back(static_cast<unsigned>(i00));
      indices->push_back(static_cast<unsigned>(i10));
      indices->push_back(static_cast<unsigned>(i11));
      indices->push_back(static_cast<unsigned>(i00));
      indices->push_back(static_cast<unsigned>(i11));
      indices->push_back(static_cast<unsigned>(i01));
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

namespace {

std::string first_existing_beside_exe(const char* const* rel, int count) {
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
  for (int i = 0; i < count; ++i) {
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

std::string find_sample_imagery_path() {
  const char* rel[] = {
      "china_rs.tif",
      "china_imagery.tif",
      "china_rs.tiff",
      "testing\\data\\china_rs.tif",
      "testing\\data\\china_imagery.tif",
      "..\\testing\\data\\china_rs.tif",
      "..\\..\\testing\\data\\china_rs.tif",
  };
  return first_existing_beside_exe(rel, static_cast<int>(sizeof(rel) / sizeof(rel[0])));
}

std::string find_sample_model_path() {
  const char* rel[] = {
      "china_model.glb",
      "china_model.gltf",
      "china_model.obj",
      "testing\\data\\china_model.glb",
      "testing\\data\\china_model.obj",
      "..\\testing\\data\\china_model.glb",
      "..\\..\\testing\\data\\china_model.glb",
  };
  return first_existing_beside_exe(rel, static_cast<int>(sizeof(rel) / sizeof(rel[0])));
}

int label_priority_from_fields(const char* name, const char* kind,
                               const char* cls, const char* adcode) {
  if (cls && std::strcmp(cls, "title") == 0) {
    return 0;
  }
  if (cls && std::strcmp(cls, "region_label") == 0) {
    return 1;
  }
  if (adcode && std::strlen(adcode) >= 6) {
    const char* tail = adcode + std::strlen(adcode) - 4;
    if (std::strcmp(tail, "0000") == 0) {
      return 1;
    }
    if (std::strcmp(tail, "0100") == 0) {
      return 2;
    }
    return 6;
  }
  if (kind &&
      (std::strcmp(kind, "region") == 0 || std::strcmp(kind, "area") == 0)) {
    return 1;
  }
  if (cls && std::strcmp(cls, "river_label") == 0) {
    return 3;
  }
  if (kind && std::strcmp(kind, "river") == 0) {
    return 3;
  }
  if (kind &&
      (std::strcmp(kind, "city") == 0 || std::strcmp(kind, "point") == 0)) {
    return 2;
  }
  if (name && *name) {
    const std::string n(name);
    if (n.find("省") != std::string::npos ||
        n.find("自治区") != std::string::npos ||
        n.find("特别行政区") != std::string::npos) {
      return 1;
    }
    if (n.size() <= 9 && n.find("市") != std::string::npos) {
      return 2;
    }
  }
  return 5;
}

int declutter_map_labels(const MapLabelBox* boxes, int count, int max_keep,
                         int view_w, int view_h, std::vector<int>* keep) {
  if (!keep) {
    return 0;
  }
  keep->clear();
  if (!boxes || count <= 0 || max_keep <= 0) {
    return 0;
  }
  std::vector<int> order(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    order[static_cast<size_t>(i)] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    if (boxes[a].priority != boxes[b].priority) {
      return boxes[a].priority < boxes[b].priority;
    }
    return a < b;
  });
  for (int idx : order) {
    const MapLabelBox& box = boxes[idx];
    if (!box_in_view(box, view_w, view_h)) {
      continue;
    }
    bool hit = false;
    for (int kept : *keep) {
      if (boxes_overlap(box, boxes[kept])) {
        hit = true;
        break;
      }
    }
    if (hit) {
      continue;
    }
    keep->push_back(idx);
    if (static_cast<int>(keep->size()) >= max_keep) {
      break;
    }
  }
  return static_cast<int>(keep->size());
}

}  // namespace render

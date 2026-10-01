// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/flood_fill.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>

#include "cpl_conv.h"
#include "gdal_priv.h"

#include <rapidjson/document.h>

namespace gis {
namespace detail {
namespace {

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out || json.empty()) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

bool json_get_double(const rapidjson::Value& obj, const char* key, double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

bool json_get_int(const rapidjson::Value& obj, const char* key, int* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

bool map_to_pixel(const double* gt, double x, double y, int* col, int* row) {
  if (!gt || !col || !row) {
    return false;
  }
  Eigen::Matrix2d a;
  a << gt[1], gt[2], gt[4], gt[5];
  if (std::abs(a.determinant()) < 1e-18) {
    return false;
  }
  const Eigen::Vector2d pix =
      a.inverse() * Eigen::Vector2d(x - gt[0], y - gt[3]);
  *col = static_cast<int>(std::floor(pix(0)));
  *row = static_cast<int>(std::floor(pix(1)));
  return true;
}

bool write_byte_geotiff(const std::string& path,
                        int width,
                        int height,
                        const double* gt,
                        const unsigned char* mask) {
  if (path.empty() || width <= 0 || height <= 0 || !gt || !mask) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(driver->Create(path.c_str(), width, height, 1,
                                         GDT_Byte, nullptr));
  if (!ds) {
    return false;
  }
  ds->SetGeoTransform(const_cast<double*>(gt));
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  band->SetNoDataValue(0);
  return band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<unsigned char*>(mask), width, height,
                        GDT_Byte, 0, 0, nullptr) == CE_None;
}

FloodFillResult flood_at_level(const std::vector<float>& elev,
                               int width,
                               int height,
                               int seed_col,
                               int seed_row,
                               double water_level) {
  FloodFillResult out;
  out.width = width;
  out.height = height;
  out.water_level = water_level;
  if (seed_col < 0 || seed_row < 0 || seed_col >= width ||
      seed_row >= height) {
    out.mask.assign(static_cast<size_t>(width) * static_cast<size_t>(height),
                    0);
    out.error = "seed_out_of_bounds";
    return out;
  }
  const size_t seed_i =
      static_cast<size_t>(seed_row) * static_cast<size_t>(width) +
      static_cast<size_t>(seed_col);
  if (!(elev[seed_i] <= water_level)) {
    out.mask.assign(static_cast<size_t>(width) * static_cast<size_t>(height),
                    0);
    out.error = "seed_above_water";
    return out;
  }
  const DemFloodSeed seed{seed_col, seed_row};
  const size_t flooded = flood_connected_at_level(
      elev, width, height, std::vector<DemFloodSeed>{seed}, water_level,
      &out.mask);
  out.ok = flooded > 0;
  if (!out.ok) {
    out.error = "empty_flood";
  }
  return out;
}

}  // namespace

size_t flood_connected_at_level(const std::vector<float>& elev,
                                int width,
                                int height,
                                const std::vector<DemFloodSeed>& seeds,
                                double water_level,
                                std::vector<unsigned char>* mask_out) {
  if (!mask_out || width <= 0 || height <= 0 ||
      elev.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
    if (mask_out) {
      mask_out->clear();
    }
    return 0;
  }
  mask_out->assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
  std::queue<std::pair<int, int>> q;
  size_t flooded = 0;
  static const int kDx[4] = {1, -1, 0, 0};
  static const int kDy[4] = {0, 0, 1, -1};
  for (const DemFloodSeed& seed : seeds) {
    if (seed.col < 0 || seed.row < 0 || seed.col >= width ||
        seed.row >= height) {
      continue;
    }
    const size_t seed_i =
        static_cast<size_t>(seed.row) * static_cast<size_t>(width) +
        static_cast<size_t>(seed.col);
    if ((*mask_out)[seed_i] || !(elev[seed_i] <= water_level) ||
        !std::isfinite(elev[seed_i])) {
      continue;
    }
    (*mask_out)[seed_i] = 1;
    q.push({seed.col, seed.row});
    ++flooded;
  }
  while (!q.empty()) {
    const auto [c, r] = q.front();
    q.pop();
    for (int k = 0; k < 4; ++k) {
      const int nc = c + kDx[k];
      const int nr = r + kDy[k];
      if (nc < 0 || nr < 0 || nc >= width || nr >= height) {
        continue;
      }
      const size_t ni =
          static_cast<size_t>(nr) * static_cast<size_t>(width) +
          static_cast<size_t>(nc);
      if ((*mask_out)[ni]) {
        continue;
      }
      if (std::isfinite(elev[ni]) && elev[ni] <= water_level) {
        (*mask_out)[ni] = 1;
        q.push({nc, nr});
        ++flooded;
      }
    }
  }
  return flooded;
}

FloodFillResult run_flood_fill(std::string_view dem_path,
                               double seed_x,
                               double seed_y,
                               double water_level,
                               int frames) {
  FloodFillResult out;
  if (dem_path.empty()) {
    out.error = "empty_dem";
    return out;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(
      GDALOpen(std::string(dem_path).c_str(), GA_ReadOnly)));
  if (!ds) {
    out.error = "open_failed";
    return out;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    out.error = "no_band";
    return out;
  }
  const int width = ds->GetRasterXSize();
  const int height = ds->GetRasterYSize();
  if (width <= 0 || height <= 0) {
    out.error = "bad_size";
    return out;
  }
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None) {
    gt[0] = 0;
    gt[1] = 1;
    gt[2] = 0;
    gt[3] = 0;
    gt[4] = 0;
    gt[5] = -1;
  }
  std::vector<float> elev(static_cast<size_t>(width) *
                          static_cast<size_t>(height));
  if (band->RasterIO(GF_Read, 0, 0, width, height, elev.data(), width, height,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
    out.error = "read_failed";
    return out;
  }
  int nodata_ok = 0;
  const double nodata = band->GetNoDataValue(&nodata_ok);
  if (nodata_ok) {
    for (float& v : elev) {
      if (std::abs(static_cast<double>(v) - nodata) < 1e-9 ||
          !std::isfinite(v)) {
        v = std::numeric_limits<float>::infinity();
      }
    }
  }

  int seed_col = 0;
  int seed_row = 0;
  if (!map_to_pixel(gt, seed_x, seed_y, &seed_col, &seed_row)) {
    out.error = "bad_geotransform";
    return out;
  }

  const int frame_count = std::max(1, frames);
  const size_t seed_i =
      static_cast<size_t>(std::clamp(seed_row, 0, height - 1)) *
          static_cast<size_t>(width) +
      static_cast<size_t>(std::clamp(seed_col, 0, width - 1));
  const float seed_z = elev[seed_i];
  if (!std::isfinite(seed_z)) {
    out.error = "seed_nodata";
    return out;
  }

  if (frame_count <= 1) {
    out = flood_at_level(elev, width, height, seed_col, seed_row, water_level);
    for (int i = 0; i < 6; ++i) {
      out.geotransform[i] = gt[i];
    }
    return out;
  }

  // Animate rising water from seed elev toward target water_level.
  const double z0 = static_cast<double>(seed_z);
  const double z1 = water_level;
  if (!(z1 >= z0)) {
    out.error = "water_below_seed";
    return out;
  }
  out.width = width;
  out.height = height;
  for (int i = 0; i < 6; ++i) {
    out.geotransform[i] = gt[i];
  }
  out.water_level = water_level;
  out.frame_masks.reserve(static_cast<size_t>(frame_count));
  for (int f = 0; f < frame_count; ++f) {
    const double t =
        frame_count == 1
            ? 1.0
            : static_cast<double>(f + 1) / static_cast<double>(frame_count);
    const double level = z0 + (z1 - z0) * t;
    FloodFillResult frame =
        flood_at_level(elev, width, height, seed_col, seed_row, level);
    if (!frame.ok) {
      out.error = frame.error.empty() ? "frame_failed" : frame.error;
      return out;
    }
    out.frame_masks.push_back(std::move(frame.mask));
  }
  out.mask = out.frame_masks.back();
  out.ok = true;
  return out;
}

bool write_flood_mask_geotiff(std::string_view output_path,
                              const FloodFillResult& result,
                              std::string_view frames_dir) {
  if (!result.ok || result.mask.empty()) {
    return false;
  }
  if (!write_byte_geotiff(std::string(output_path), result.width, result.height,
                          result.geotransform, result.mask.data())) {
    return false;
  }
  if (frames_dir.empty() || result.frame_masks.empty()) {
    return true;
  }
  for (size_t i = 0; i < result.frame_masks.size(); ++i) {
    const std::string frame_path =
        std::string(frames_dir) + "/frame_" + std::to_string(i) + ".tif";
    if (!write_byte_geotiff(frame_path, result.width, result.height,
                            result.geotransform,
                            result.frame_masks[i].data())) {
      return false;
    }
  }
  return true;
}

bool run_flood_fill_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string dem;
  std::string output;
  if (!json_get_string(args, "dem", &dem) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  double seed_x = 0;
  double seed_y = 0;
  if (!json_get_double(args, "seed_x", &seed_x) ||
      !json_get_double(args, "seed_y", &seed_y)) {
    return false;
  }
  double water_level = 0;
  double water_depth = 0;
  const bool has_level = json_get_double(args, "water_level", &water_level);
  const bool has_depth = json_get_double(args, "water_depth", &water_depth);
  if (!has_level && !has_depth) {
    return false;
  }

  // Resolve absolute level when only depth is provided: sample DEM at seed.
  if (!has_level) {
    GDALAllRegister();
    GDALDatasetUniquePtr ds(
        static_cast<GDALDataset*>(GDALOpen(dem.c_str(), GA_ReadOnly)));
    if (!ds) {
      return false;
    }
    double gt[6] = {};
    if (ds->GetGeoTransform(gt) != CE_None) {
      return false;
    }
    int col = 0;
    int row = 0;
    if (!map_to_pixel(gt, seed_x, seed_y, &col, &row)) {
      return false;
    }
    GDALRasterBand* band = ds->GetRasterBand(1);
    float z = 0;
    if (!band ||
        band->RasterIO(GF_Read, col, row, 1, 1, &z, 1, 1, GDT_Float32, 0, 0,
                       nullptr) != CE_None) {
      return false;
    }
    water_level = static_cast<double>(z) + water_depth;
  }

  int frames = 1;
  json_get_int(args, "frames", &frames);
  std::string frames_dir;
  json_get_string(args, "frames_dir", &frames_dir);

  const FloodFillResult result =
      run_flood_fill(dem, seed_x, seed_y, water_level, frames);
  if (!result.ok) {
    return false;
  }
  return write_flood_mask_geotiff(output, result, frames_dir);
}

}  // namespace detail
}  // namespace gis

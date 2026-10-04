// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/dem_gradient.h"

#include "gis/analysis/raster/dem/dem_gradient_profile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include <Eigen/Core>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"

#include "cpl_conv.h"
#include "gdal_priv.h"

#include <rapidjson/document.h>

namespace gis {
namespace detail {
namespace {

constexpr double kPi = 3.14159265358979323846;

bool should_parallel_gradient(int width, int height) {
  const int mode = dem_gradient_dispatch_override();
  if (mode == kGradientDispatchSerial) {
    return false;
  }
  if (mode == kGradientDispatchParallel) {
    return true;
  }
  const size_t pixels =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  return height >= kParallelGradientMinRows &&
         pixels >= static_cast<size_t>(kParallelGradientMinPixels);
}

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

bool write_float_geotiff(const std::string& path,
                         int width,
                         int height,
                         const double* gt,
                         const float* data) {
  if (path.empty() || width <= 0 || height <= 0 || !gt || !data) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), width, height, 1, GDT_Float32, nullptr));
  if (!ds) {
    return false;
  }
  ds->SetGeoTransform(const_cast<double*>(gt));
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  return band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<float*>(data), width, height, GDT_Float32, 0,
                        0, nullptr) == CE_None;
}

}  // namespace

DemGradientResult compute_dem_gradient(const std::vector<float>& elev,
                                       int width,
                                       int height,
                                       double cell_x,
                                       double cell_y) {
  DemGradientResult out;
  if (width < 2 || height < 2 ||
      elev.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
    out.error = "bad_size";
    return out;
  }
  const double cx = std::abs(cell_x) < 1e-18 ? 1.0 : std::abs(cell_x);
  const double cy = std::abs(cell_y) < 1e-18 ? 1.0 : std::abs(cell_y);
  out.width = width;
  out.height = height;
  out.slope_deg.assign(elev.size(), 0.f);
  out.aspect_deg.assign(elev.size(), 0.f);

  using Clock = std::chrono::steady_clock;
  const auto t_dispatch0 = Clock::now();

  using Map = Eigen::Map<const Eigen::Matrix<float, Eigen::Dynamic,
                                             Eigen::Dynamic, Eigen::RowMajor>>;
  const Map grid(elev.data(), height, width);
  float* slope = out.slope_deg.data();
  float* aspect = out.aspect_deg.data();

  const bool use_parallel = should_parallel_gradient(width, height);
  const char* backend = use_parallel ? "parallel" : "serial";
  const auto t_dispatch1 = Clock::now();

  auto fill_row = [&](int r) {
    for (int c = 0; c < width; ++c) {
      const int c0 = (std::max)(0, c - 1);
      const int c1 = (std::min)(width - 1, c + 1);
      const int r0 = (std::max)(0, r - 1);
      const int r1 = (std::min)(height - 1, r + 1);
      const double dzdx =
          (static_cast<double>(grid(r, c1)) - static_cast<double>(grid(r, c0))) /
          (cx * static_cast<double>(c1 - c0));
      const double dzdy =
          (static_cast<double>(grid(r1, c)) - static_cast<double>(grid(r0, c))) /
          (cy * static_cast<double>(r1 - r0));
      const double slope_rad = std::atan(std::hypot(dzdx, dzdy));
      double aspect_rad = std::atan2(dzdy, -dzdx);
      if (aspect_rad < 0.0) {
        aspect_rad += 2.0 * kPi;
      }
      const size_t i =
          static_cast<size_t>(r) * static_cast<size_t>(width) +
          static_cast<size_t>(c);
      slope[i] = static_cast<float>(slope_rad * 180.0 / kPi);
      aspect[i] = static_cast<float>(aspect_rad * 180.0 / kPi);
    }
  };

  const auto t_compute0 = Clock::now();
  if (use_parallel) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, height, fill_row);
  } else {
    for (int r = 0; r < height; ++r) {
      fill_row(r);
    }
  }
  const auto t_compute1 = Clock::now();

  DemGradientProfile snap;
  snap.width = width;
  snap.height = height;
  snap.pixels = width * height;
  snap.threads = use_parallel ? kDemGradientPoolThreads : 1;
  snap.dispatch_ms =
      std::chrono::duration<double, std::milli>(t_dispatch1 - t_dispatch0)
          .count();
  snap.compute_ms =
      std::chrono::duration<double, std::milli>(t_compute1 - t_compute0).count();
  snap.backend = backend;
  record_dem_gradient_profile(snap);

  out.ok = true;
  return out;
}

DemGradientResult run_dem_gradient(std::string_view dem_path) {
  DemGradientResult out;
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
  out = compute_dem_gradient(elev, width, height, gt[1], gt[5]);
  for (int i = 0; i < 6; ++i) {
    out.geotransform[i] = gt[i];
  }
  return out;
}

bool write_dem_gradient_geotiff(std::string_view slope_path,
                                std::string_view aspect_path,
                                const DemGradientResult& result) {
  if (!result.ok) {
    return false;
  }
  if (!slope_path.empty() &&
      !write_float_geotiff(std::string(slope_path), result.width, result.height,
                           result.geotransform, result.slope_deg.data())) {
    return false;
  }
  if (!aspect_path.empty() &&
      !write_float_geotiff(std::string(aspect_path), result.width,
                           result.height, result.geotransform,
                           result.aspect_deg.data())) {
    return false;
  }
  return !slope_path.empty() || !aspect_path.empty();
}

bool run_dem_gradient_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string dem;
  std::string slope_output;
  if (!json_get_string(args, "dem", &dem) ||
      !json_get_string(args, "slope_output", &slope_output)) {
    return false;
  }
  std::string aspect_output;
  json_get_string(args, "aspect_output", &aspect_output);
  const DemGradientResult result = run_dem_gradient(dem);
  if (!result.ok) {
    return false;
  }
  return write_dem_gradient_geotiff(slope_output, aspect_output, result);
}

}  // namespace detail
}  // namespace gis

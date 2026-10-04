// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/filter/raster_convolve.h"

#include "gis/analysis/raster/filter/raster_convolve_profile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"

#include "cpl_conv.h"
#include "gdal_priv.h"

#include <rapidjson/document.h>

namespace gis {
namespace detail {
namespace {

std::atomic<int> g_dispatch_override{kConvolveDispatchAuto};
RasterConvolveProfile g_last_profile;
std::mutex g_last_profile_mu;

void record_convolve_profile(const RasterConvolveProfile& snap) {
  {
    std::lock_guard<std::mutex> lock(g_last_profile_mu);
    g_last_profile = snap;
  }
  if (!analysis_profile_enabled()) {
    return;
  }
  std::fprintf(stderr,
               "raster_convolve kernel_size=%d (%dx%d) pixels=%d "
               "dispatch_ms=%.4f compute_ms=%.4f backend=%s\n",
               snap.kernel_size, snap.kernel_w, snap.kernel_h, snap.pixels,
               snap.dispatch_ms, snap.compute_ms, snap.backend);
}

bool should_parallel_convolve(int width, int height) {
  const int mode = convolve_dispatch_override();
  if (mode == kConvolveDispatchSerial) {
    return false;
  }
  if (mode == kConvolveDispatchParallel) {
    return true;
  }
  const size_t pixels =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  return height >= kParallelConvolveMinRows &&
         pixels >= static_cast<size_t>(kParallelConvolveMinPixels);
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

bool analysis_profile_enabled() {
  const char* e = std::getenv("SMT_ANALYSIS_PROFILE");
  if (!e || !e[0]) {
    return false;
  }
  return std::strcmp(e, "1") == 0 || std::strcmp(e, "true") == 0 ||
         std::strcmp(e, "TRUE") == 0 || std::strcmp(e, "on") == 0 ||
         std::strcmp(e, "ON") == 0;
}

void set_convolve_dispatch_override(int mode) {
  g_dispatch_override.store(mode, std::memory_order_relaxed);
}

int convolve_dispatch_override() {
  return g_dispatch_override.load(std::memory_order_relaxed);
}

RasterConvolveProfile last_convolve_profile() {
  std::lock_guard<std::mutex> lock(g_last_profile_mu);
  return g_last_profile;
}

RasterConvolveResult convolve_raster(const std::vector<float>& input,
                                     int width,
                                     int height,
                                     const std::vector<double>& kernel,
                                     int kernel_w,
                                     int kernel_h) {
  RasterConvolveResult out;
  if (width <= 0 || height <= 0 || kernel_w <= 0 || kernel_h <= 0 ||
      (kernel_w % 2) == 0 || (kernel_h % 2) == 0 ||
      kernel.size() != static_cast<size_t>(kernel_w) * static_cast<size_t>(kernel_h) ||
      input.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
    out.error = "bad_args";
    return out;
  }
  using Clock = std::chrono::steady_clock;
  const auto t_dispatch0 = Clock::now();

  using MapF = Eigen::Map<
      const Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
  using MapK = Eigen::Map<
      const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
  const MapF src(input.data(), height, width);
  const MapK ker(kernel.data(), kernel_h, kernel_w);
  const int rx = kernel_w / 2;
  const int ry = kernel_h / 2;
  out.width = width;
  out.height = height;
  out.values.assign(input.size(), 0.f);
  Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
      dst(out.values.data(), height, width);

  const bool use_parallel = should_parallel_convolve(width, height);
  const char* backend = use_parallel ? "parallel" : "serial";
  const auto t_dispatch1 = Clock::now();

  auto convolve_row = [&](int r) {
    for (int c = 0; c < width; ++c) {
      double acc = 0.0;
      for (int ky = 0; ky < kernel_h; ++ky) {
        for (int kx = 0; kx < kernel_w; ++kx) {
          const int sr = (std::clamp)(r + ky - ry, 0, height - 1);
          const int sc = (std::clamp)(c + kx - rx, 0, width - 1);
          acc += static_cast<double>(src(sr, sc)) * ker(ky, kx);
        }
      }
      dst(r, c) = static_cast<float>(acc);
    }
  };

  const auto t_compute0 = Clock::now();
  // Parallelize by output row; each row writes a disjoint dst slice.
  if (use_parallel) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, height, convolve_row);
  } else {
    for (int r = 0; r < height; ++r) {
      convolve_row(r);
    }
  }
  const auto t_compute1 = Clock::now();

  RasterConvolveProfile snap;
  snap.kernel_w = kernel_w;
  snap.kernel_h = kernel_h;
  snap.kernel_size = kernel_w * kernel_h;
  snap.pixels = width * height;
  snap.dispatch_ms =
      std::chrono::duration<double, std::milli>(t_dispatch1 - t_dispatch0)
          .count();
  snap.compute_ms =
      std::chrono::duration<double, std::milli>(t_compute1 - t_compute0).count();
  snap.backend = backend;
  record_convolve_profile(snap);

  out.ok = true;
  return out;
}

RasterConvolveResult convolve_box3(const std::vector<float>& input,
                                   int width,
                                   int height) {
  const std::vector<double> k(9, 1.0 / 9.0);
  return convolve_raster(input, width, height, k, 3, 3);
}

bool run_raster_convolve_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(
      static_cast<GDALDataset*>(GDALOpen(input.c_str(), GA_ReadOnly)));
  if (!ds) {
    return false;
  }
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  const int width = ds->GetRasterXSize();
  const int height = ds->GetRasterYSize();
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None) {
    gt[1] = 1;
    gt[5] = -1;
  }
  std::vector<float> elev(static_cast<size_t>(width) *
                          static_cast<size_t>(height));
  if (band->RasterIO(GF_Read, 0, 0, width, height, elev.data(), width, height,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
    return false;
  }
  RasterConvolveResult result = convolve_box3(elev, width, height);
  if (!result.ok) {
    return false;
  }
  for (int i = 0; i < 6; ++i) {
    result.geotransform[i] = gt[i];
  }
  return write_float_geotiff(output, result.width, result.height,
                             result.geotransform, result.values.data());
}

}  // namespace detail
}  // namespace gis

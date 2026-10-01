// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/idw.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/geochem/stats.h"

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

bool write_float_geotiff(const std::string& path,
                         int width,
                         int height,
                         const double* gt,
                         const float* data) {
  if (path.empty() || !gt || !data || width <= 0 || height <= 0) {
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
  band->SetNoDataValue(-9999.0);
  return band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<float*>(data), width, height, GDT_Float32, 0,
                        0, nullptr) == CE_None;
}

bool write_byte_geotiff(const std::string& path,
                        int width,
                        int height,
                        const double* gt,
                        const unsigned char* data) {
  if (path.empty() || !gt || !data || width <= 0 || height <= 0) {
    return false;
  }
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), width, height, 1, GDT_Byte, nullptr));
  if (!ds) {
    return false;
  }
  ds->SetGeoTransform(const_cast<double*>(gt));
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (!band) {
    return false;
  }
  return band->RasterIO(GF_Write, 0, 0, width, height,
                        const_cast<unsigned char*>(data), width, height,
                        GDT_Byte, 0, 0, nullptr) == CE_None;
}

}  // namespace

GeochemIdwResult run_geochem_idw(const GeochemSampleSet& set,
                                 std::string_view element,
                                 int cell_count,
                                 double power,
                                 double threshold,
                                 double k_sigma) {
  GeochemIdwResult out;
  out.element = std::string(element);
  if (!set.ok || set.samples.empty()) {
    out.error = set.error.empty() ? "bad_samples" : set.error;
    return out;
  }
  const int idx = geochem_element_index(set, element);
  if (idx < 0) {
    out.error = "element_missing";
    return out;
  }
  if (cell_count < 8) {
    cell_count = 64;
  }
  if (!(power > 0.0)) {
    power = 2.0;
  }

  struct Pt {
    double x;
    double y;
    double v;
  };
  std::vector<Pt> pts;
  pts.reserve(set.samples.size());
  double min_x = 0;
  double max_x = 0;
  double min_y = 0;
  double max_y = 0;
  bool first = true;
  for (const GeochemSample& s : set.samples) {
    if (static_cast<size_t>(idx) >= s.values.size()) {
      continue;
    }
    const double v = s.values[static_cast<size_t>(idx)];
    if (!std::isfinite(v)) {
      continue;
    }
    pts.push_back({s.x, s.y, v});
    if (first) {
      min_x = max_x = s.x;
      min_y = max_y = s.y;
      first = false;
    } else {
      min_x = std::min(min_x, s.x);
      max_x = std::max(max_x, s.x);
      min_y = std::min(min_y, s.y);
      max_y = std::max(max_y, s.y);
    }
  }
  if (pts.size() < 3) {
    out.error = "too_few_samples";
    return out;
  }

  const double dx = std::max(1e-9, max_x - min_x);
  const double dy = std::max(1e-9, max_y - min_y);
  const double pad_x = 0.05 * dx;
  const double pad_y = 0.05 * dy;
  min_x -= pad_x;
  max_x += pad_x;
  min_y -= pad_y;
  max_y += pad_y;

  const double aspect = (max_x - min_x) / (max_y - min_y);
  int width = cell_count;
  int height = cell_count;
  if (aspect >= 1.0) {
    height = std::max(8, static_cast<int>(std::lround(cell_count / aspect)));
  } else {
    width = std::max(8, static_cast<int>(std::lround(cell_count * aspect)));
  }

  const double px = (max_x - min_x) / static_cast<double>(width);
  const double py = (max_y - min_y) / static_cast<double>(height);
  // North-up GeoTransform (origin UL).
  out.geotransform[0] = min_x;
  out.geotransform[1] = px;
  out.geotransform[2] = 0;
  out.geotransform[3] = max_y;
  out.geotransform[4] = 0;
  out.geotransform[5] = -py;
  out.width = width;
  out.height = height;
  out.values.assign(static_cast<size_t>(width * height), -9999.0f);

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      const double cx = min_x + (static_cast<double>(col) + 0.5) * px;
      const double cy = max_y - (static_cast<double>(row) + 0.5) * py;
      double wsum = 0;
      double vsum = 0;
      bool exact = false;
      for (const Pt& p : pts) {
        const double ddx = cx - p.x;
        const double ddy = cy - p.y;
        const double dist2 = ddx * ddx + ddy * ddy;
        if (dist2 < 1e-18) {
          out.values[static_cast<size_t>(row * width + col)] =
              static_cast<float>(p.v);
          exact = true;
          break;
        }
        const double w = 1.0 / std::pow(std::sqrt(dist2), power);
        wsum += w;
        vsum += w * p.v;
      }
      if (!exact && wsum > 0.0) {
        out.values[static_cast<size_t>(row * width + col)] =
            static_cast<float>(vsum / wsum);
      }
    }
  }

  if (!std::isfinite(threshold)) {
    const GeochemElementStats st =
        compute_geochem_stats(set, element, 10, k_sigma > 0 ? k_sigma : 2.0);
    threshold = st.ok ? st.threshold : 0.0;
  }
  out.threshold = threshold;
  out.anomaly_mask.assign(static_cast<size_t>(width * height), 0);
  for (size_t i = 0; i < out.values.size(); ++i) {
    const float v = out.values[i];
    if (std::isfinite(v) && v >= static_cast<float>(threshold)) {
      out.anomaly_mask[i] = 1;
    }
  }
  out.ok = true;
  return out;
}

bool write_geochem_idw_geotiff(std::string_view output_path,
                               const GeochemIdwResult& result,
                               std::string_view mask_path) {
  if (!result.ok || result.values.empty()) {
    return false;
  }
  if (!write_float_geotiff(std::string(output_path), result.width, result.height,
                           result.geotransform, result.values.data())) {
    return false;
  }
  if (!mask_path.empty()) {
    return write_byte_geotiff(std::string(mask_path), result.width,
                              result.height, result.geotransform,
                              result.anomaly_mask.data());
  }
  return true;
}

bool run_geochem_idw_op(std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    return false;
  }
  std::string input;
  std::string element;
  std::string output;
  if (!json_get_string(args, "input", &input) ||
      !json_get_string(args, "element", &element) ||
      !json_get_string(args, "output", &output)) {
    return false;
  }
  int cells = 64;
  double power = 2.0;
  double k_sigma = 2.0;
  json_get_int(args, "cells", &cells);
  json_get_double(args, "power", &power);
  json_get_double(args, "k_sigma", &k_sigma);
  double threshold = std::numeric_limits<double>::quiet_NaN();
  json_get_double(args, "threshold", &threshold);
  std::string mask_out;
  json_get_string(args, "mask_output", &mask_out);

  GeochemSampleSet set = load_geochem_csv(input);
  if (!set.ok) {
    return false;
  }
  const GeochemIdwResult result =
      run_geochem_idw(set, element, cells, power, threshold, k_sigma);
  if (!result.ok) {
    return false;
  }
  return write_geochem_idw_geotiff(output, result, mask_out);
}

}  // namespace detail
}  // namespace gis

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/filter/raster_smooth.h"

#include <string>
#include <vector>

#include <Eigen/Sparse>
#include <Eigen/SparseLU>

#include "cpl_conv.h"
#include "gdal_priv.h"

#include <rapidjson/document.h>

namespace gis {
namespace detail {
namespace {

using Triplet = Eigen::Triplet<double>;
using SpMat = Eigen::SparseMatrix<double>;

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

int node_index(int width, int c, int r) {
  return r * width + c;
}

}  // namespace

RasterSmoothResult smooth_raster_laplace(const std::vector<float>& values,
                                         int width,
                                         int height,
                                         const std::uint8_t* is_unknown) {
  RasterSmoothResult out;
  if (width < 3 || height < 3 || !is_unknown ||
      values.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) {
    out.error = "bad_args";
    return out;
  }
  out.width = width;
  out.height = height;
  out.values = values;

  const int n = width * height;
  std::vector<int> unk_id(static_cast<size_t>(n), -1);
  int nunk = 0;
  for (int k = 0; k < n; ++k) {
    if (is_unknown[k] != 0) {
      unk_id[static_cast<size_t>(k)] = nunk++;
    }
  }
  if (nunk == 0) {
    out.ok = true;
    return out;
  }

  std::vector<Triplet> trips;
  trips.reserve(static_cast<size_t>(nunk) * 5u);
  Eigen::VectorXd b = Eigen::VectorXd::Zero(nunk);
  static const int kDc[4] = {1, -1, 0, 0};
  static const int kDr[4] = {0, 0, 1, -1};

  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      const int k = node_index(width, c, r);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      double diag = 0.0;
      for (int nbi = 0; nbi < 4; ++nbi) {
        const int nc = c + kDc[nbi];
        const int nr = r + kDr[nbi];
        if (nc < 0 || nr < 0 || nc >= width || nr >= height) {
          continue;
        }
        const int nk = node_index(width, nc, nr);
        const int col = unk_id[static_cast<size_t>(nk)];
        diag += 1.0;
        if (col >= 0) {
          trips.emplace_back(row, col, -1.0);
        } else {
          b(row) += static_cast<double>(out.values[static_cast<size_t>(nk)]);
        }
      }
      if (diag <= 0.0) {
        out.error = "isolated_unknown";
        return out;
      }
      trips.emplace_back(row, row, diag);
    }
  }

  SpMat a(nunk, nunk);
  a.setFromTriplets(trips.begin(), trips.end());
  a.makeCompressed();
  Eigen::SparseLU<SpMat> solver;
  solver.analyzePattern(a);
  solver.factorize(a);
  if (solver.info() != Eigen::Success) {
    out.error = "factorize_failed";
    return out;
  }
  const Eigen::VectorXd x = solver.solve(b);
  if (solver.info() != Eigen::Success) {
    out.error = "solve_failed";
    return out;
  }
  for (int k = 0; k < n; ++k) {
    const int row = unk_id[static_cast<size_t>(k)];
    if (row >= 0) {
      out.values[static_cast<size_t>(k)] = static_cast<float>(x(row));
    }
  }
  out.ok = true;
  return out;
}

bool run_raster_smooth_op(std::string_view args_json) {
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
  // Optional mask: 1 = unknown. If omitted, interior cells are unknown.
  std::string mask_path;
  json_get_string(args, "mask", &mask_path);

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
  std::vector<float> values(static_cast<size_t>(width) *
                            static_cast<size_t>(height));
  if (band->RasterIO(GF_Read, 0, 0, width, height, values.data(), width, height,
                     GDT_Float32, 0, 0, nullptr) != CE_None) {
    return false;
  }
  std::vector<std::uint8_t> unknown(
      static_cast<size_t>(width) * static_cast<size_t>(height), 0);
  if (!mask_path.empty()) {
    GDALDatasetUniquePtr mds(static_cast<GDALDataset*>(
        GDALOpen(mask_path.c_str(), GA_ReadOnly)));
    if (!mds || mds->GetRasterXSize() != width ||
        mds->GetRasterYSize() != height) {
      return false;
    }
    GDALRasterBand* mb = mds->GetRasterBand(1);
    if (!mb ||
        mb->RasterIO(GF_Read, 0, 0, width, height, unknown.data(), width,
                     height, GDT_Byte, 0, 0, nullptr) != CE_None) {
      return false;
    }
  } else {
    for (int r = 1; r < height - 1; ++r) {
      for (int c = 1; c < width - 1; ++c) {
        unknown[static_cast<size_t>(r * width + c)] = 1;
      }
    }
  }

  RasterSmoothResult result =
      smooth_raster_laplace(values, width, height, unknown.data());
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

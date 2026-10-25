// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/io/geotiff.h"

#include <string>

#include "cpl_conv.h"
#include "gdal_priv.h"

namespace gis {
namespace detail {
namespace {

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

bool write_idw_geotiff(std::string_view output_path,
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

}  // namespace detail
}  // namespace gis

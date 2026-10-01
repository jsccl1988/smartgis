// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/raster/dem/flood_fill.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string temp_file(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

std::string write_toy_dem() {
  const std::string path = temp_file("flood_inundate_test_dem.tif");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  expect(driver != nullptr, "gtiff driver");
  if (!driver) {
    return {};
  }
  VSIUnlink(path.c_str());
  constexpr int kW = 8;
  constexpr int kH = 8;
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), kW, kH, 1, GDT_Float32, nullptr));
  expect(!!ds, "create dem");
  if (!ds) {
    return {};
  }
  double gt[6] = {0, 1, 0, 0, 0, -1};
  ds->SetGeoTransform(gt);
  std::vector<float> elev(static_cast<size_t>(kW * kH), 10.0f);
  // Basin around (2,2) / map (2,-2)
  elev[2 * kW + 2] = 1.0f;
  elev[2 * kW + 3] = 2.0f;
  elev[3 * kW + 2] = 2.0f;
  elev[3 * kW + 3] = 3.0f;
  GDALRasterBand* band = ds->GetRasterBand(1);
  expect(band != nullptr, "band");
  if (!band) {
    return {};
  }
  expect(band->RasterIO(GF_Write, 0, 0, kW, kH, elev.data(), kW, kH,
                        GDT_Float32, 0, 0, nullptr) == CE_None,
         "write elev");
  return path;
}

}  // namespace

int main() {
  const std::string dem = write_toy_dem();
  expect(!dem.empty(), "dem path");
  // Seed at pixel (2,2) → map x=2.5,y=-2.5 with gt origin (0,0) px size 1/-1
  const gis::detail::FloodFillResult result =
      gis::detail::run_flood_fill(dem, 2.5, -2.5, 2.5, 3);
  expect(result.ok, "flood ok");
  expect(result.width == 8 && result.height == 8, "size");
  expect(!result.mask.empty(), "mask");
  expect(result.frame_masks.size() == 3, "frames");
  const std::string out = temp_file("flood_inundate_test_mask.tif");
  expect(gis::detail::write_flood_mask_geotiff(out, result, ""), "write mask");
  expect(gis::detail::run_flood_fill_op(
             ("{\"dem\":\"" + dem + "\",\"output\":\"" + out +
              "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"water_level\":2.5,"
              "\"frames\":1}")
                 .c_str()),
         "op runner");
  std::error_code ec;
  std::filesystem::remove(dem, ec);
  std::filesystem::remove(out, ec);
  if (g_fails == 0) {
    std::printf("flood_inundate_test OK\n");
  }
  return g_fails == 0 ? 0 : 1;
}

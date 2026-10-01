// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/raster/dem/storm_surge.h"

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
  const std::string path = temp_file("stormsurge_run_test_dem.tif");
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

std::string write_toy_coast() {
  const std::string path = temp_file("stormsurge_run_test_coast.geojson");
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
    expect(false, "open coast");
    return {};
  }
  const char* json =
      "{\"type\":\"FeatureCollection\",\"features\":[{"
      "\"type\":\"Feature\",\"properties\":{},\"geometry\":{"
      "\"type\":\"LineString\",\"coordinates\":[[2.5,-2.5],[3.5,-2.5]]}}]}";
  std::fputs(json, f);
  std::fclose(f);
  return path;
}

}  // namespace

int main() {
  const std::string dem = write_toy_dem();
  const std::string coast = write_toy_coast();
  expect(!dem.empty() && !coast.empty(), "fixtures");
  const std::vector<double> seeds = {2.5, -2.5};
  const std::vector<double> levels = {2.5};
  const gis::detail::StormSurgeResult result =
      gis::detail::run_storm_surge(dem, coast, seeds, levels, 3);
  expect(result.ok, "stormsurge kernel ok");
  expect(result.width == 8 && result.height == 8, "size");
  expect(!result.mask.empty(), "mask");
  expect(result.frame_masks.size() == 3, "frames");
  const gis::detail::StormSurgeWaterMesh mesh =
      gis::detail::build_storm_surge_water_mesh(result, -1);
  expect(mesh.xyz.size() >= 9 && mesh.indices.size() >= 3, "water mesh");
  const std::string out = temp_file("stormsurge_run_test_mask.tif");
  expect(gis::detail::write_storm_surge_mask_geotiff(out, result, ""),
         "write mask");
  expect(gis::detail::run_storm_surge_op(
             ("{\"dem\":\"" + dem + "\",\"coast\":\"" + coast +
              "\",\"output\":\"" + out +
              "\",\"seed_x\":2.5,\"seed_y\":-2.5,\"water_level\":2.5,"
              "\"frames\":1}")
                 .c_str()),
         "op runner");
  std::error_code ec;
  std::filesystem::remove(dem, ec);
  std::filesystem::remove(coast, ec);
  std::filesystem::remove(out, ec);
  if (g_fails == 0) {
    std::printf("stormsurge_run_test OK\n");
  }
  return g_fails == 0 ? 0 : 1;
}

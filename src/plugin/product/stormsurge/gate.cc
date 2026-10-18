// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/gate.h"

#include <filesystem>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/raster/dem/storm_surge.h"

namespace plugin {
namespace {

struct SurgeGateFix {
  std::string dem;
  std::string coast;
  std::string out;
  gis::detail::StormSurgeResult result;
};

SurgeGateFix& fix() {
  static SurgeGateFix state;
  return state;
}

std::string temp_file(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

void clear_fix() {
  std::error_code ec;
  if (!fix().dem.empty()) {
    std::filesystem::remove(fix().dem, ec);
  }
  if (!fix().coast.empty()) {
    std::filesystem::remove(fix().coast, ec);
  }
  if (!fix().out.empty()) {
    std::filesystem::remove(fix().out, ec);
  }
  fix() = {};
}

bool surge_contract(void*) {
  const std::string dem = temp_file("stormsurge_gate_dem.tif");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (driver == nullptr) {
    return false;
  }
  VSIUnlink(dem.c_str());
  constexpr int kW = 8;
  constexpr int kH = 8;
  GDALDatasetUniquePtr ds(
      driver->Create(dem.c_str(), kW, kH, 1, GDT_Float32, nullptr));
  if (!ds) {
    return false;
  }
  double gt[6] = {0, 1, 0, 0, 0, -1};
  ds->SetGeoTransform(gt);
  std::vector<float> elev(static_cast<size_t>(kW * kH), 10.0f);
  elev[2 * kW + 2] = 1.0f;
  elev[2 * kW + 3] = 2.0f;
  elev[3 * kW + 2] = 2.0f;
  elev[3 * kW + 3] = 3.0f;
  GDALRasterBand* band = ds->GetRasterBand(1);
  if (band == nullptr ||
      band->RasterIO(GF_Write, 0, 0, kW, kH, elev.data(), kW, kH, GDT_Float32,
                     0, 0, nullptr) != CE_None) {
    return false;
  }
  ds.reset();

  const std::string coast = temp_file("stormsurge_gate_coast.geojson");
  FILE* file = nullptr;
  if (fopen_s(&file, coast.c_str(), "wb") != 0 || file == nullptr) {
    return false;
  }
  const char* json =
      "{\"type\":\"FeatureCollection\",\"features\":[{"
      "\"type\":\"Feature\",\"properties\":{},\"geometry\":{"
      "\"type\":\"LineString\",\"coordinates\":[[2.5,-2.5],[3.5,-2.5]]}}]}";
  const int wrote = std::fputs(json, file);
  std::fclose(file);
  if (wrote < 0) {
    return false;
  }
  fix().dem = dem;
  fix().coast = coast;
  return true;
}

bool surge_compute(void*) {
  const std::vector<double> seeds = {2.5, -2.5};
  const std::vector<double> levels = {2.5};
  fix().result = gis::detail::run_storm_surge(fix().dem, fix().coast, seeds,
                                              levels, 3);
  if (!fix().result.ok || fix().result.width != 8 || fix().result.height != 8 ||
      fix().result.mask.empty() || fix().result.frame_masks.size() != 3) {
    return false;
  }
  const gis::detail::StormSurgeWaterMesh mesh =
      gis::detail::build_storm_surge_water_mesh(fix().result, -1);
  return mesh.xyz.size() >= 9 && mesh.indices.size() >= 3;
}

bool surge_command(void*) {
  fix().out = temp_file("stormsurge_gate_mask.tif");
  const std::string args = "{\"dem\":\"" + fix().dem + "\",\"coast\":\"" +
                           fix().coast + "\",\"output\":\"" + fix().out +
                           "\",\"seed_x\":2.5,\"seed_y\":-2.5,"
                           "\"water_level\":2.5,\"frames\":1}";
  return gis::detail::run_storm_surge_op(args.c_str());
}

bool surge_visual(void*) {
  std::error_code ec;
  if (fix().out.empty() || !std::filesystem::is_regular_file(fix().out, ec) ||
      ec || std::filesystem::file_size(fix().out, ec) == 0 || ec) {
    return false;
  }
  return gis::detail::write_storm_surge_mask_geotiff(fix().out, fix().result,
                                                     "");
}

}  // namespace

ProductGateResult run_stormsurge_product_gate() {
  clear_fix();
  static const ProductGateStep kSteps[] = {
      {"contract", surge_contract},
      {"compute", surge_compute},
      {"command", surge_command},
      {"visual", surge_visual},
  };
  const ProductGateResult result =
      run_product_gate("stormsurge", kSteps, 4, nullptr);
  clear_fix();
  return result;
}

}  // namespace plugin

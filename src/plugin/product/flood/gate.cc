// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/gate.h"

#include <filesystem>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/raster/dem/flood_fill.h"

namespace plugin {
namespace {

struct FloodGateFix {
  std::string dem;
  std::string out;
};

FloodGateFix& fix() {
  static FloodGateFix state;
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
  if (!fix().out.empty()) {
    std::filesystem::remove(fix().out, ec);
  }
  fix() = {};
}

bool flood_contract(void*) {
  const std::string path = temp_file("flood_gate_dem.tif");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (driver == nullptr) {
    return false;
  }
  VSIUnlink(path.c_str());
  constexpr int kW = 8;
  constexpr int kH = 8;
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), kW, kH, 1, GDT_Float32, nullptr));
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
  if (band == nullptr) {
    return false;
  }
  if (band->RasterIO(GF_Write, 0, 0, kW, kH, elev.data(), kW, kH, GDT_Float32,
                     0, 0, nullptr) != CE_None) {
    return false;
  }
  ds.reset();
  fix().dem = path;
  return !fix().dem.empty();
}

bool flood_compute(void*) {
  const gis::detail::FloodFillResult result =
      gis::detail::run_flood_fill(fix().dem, 2.5, -2.5, 2.5, 3);
  return result.ok && result.width == 8 && result.height == 8 &&
         !result.mask.empty() && result.frame_masks.size() == 3;
}

bool flood_command(void*) {
  fix().out = temp_file("flood_gate_mask.tif");
  const std::string args = "{\"dem\":\"" + fix().dem + "\",\"output\":\"" +
                           fix().out +
                           "\",\"seed_x\":2.5,\"seed_y\":-2.5,"
                           "\"water_level\":2.5,\"frames\":1}";
  return gis::detail::run_flood_fill_op(args.c_str());
}

bool flood_visual(void*) {
  std::error_code ec;
  return !fix().out.empty() && std::filesystem::is_regular_file(fix().out, ec) &&
         std::filesystem::file_size(fix().out, ec) > 0 && !ec;
}

}  // namespace

ProductGateResult run_flood_product_gate() {
  clear_fix();
  static const ProductGateStep kSteps[] = {
      {"contract", flood_contract},
      {"compute", flood_compute},
      {"command", flood_command},
      {"visual", flood_visual},
  };
  const ProductGateResult result =
      run_product_gate("flood", kSteps, 4, nullptr);
  clear_fix();
  return result;
}

}  // namespace plugin

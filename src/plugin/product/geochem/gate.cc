// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/geochem/gate.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/samples.h"
#include "gis/analysis/geochem/stats.h"
#include "third_party/gn/dll_search.h"

namespace plugin {
namespace {

struct GeochemGateFix {
  std::string csv;
  std::string raster;
  gis::detail::GeochemSampleSet samples;
};

GeochemGateFix& fix() {
  static GeochemGateFix state;
  return state;
}

void clear_fix() {
  std::error_code ec;
  if (!fix().csv.empty()) {
    std::filesystem::remove(fix().csv, ec);
  }
  if (!fix().raster.empty()) {
    std::filesystem::remove(fix().raster, ec);
  }
  fix() = {};
}

bool geochem_contract(void*) {
  const std::string path =
      (std::filesystem::temp_directory_path() / "geochem_gate.csv")
          .generic_string();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << "id,lon,lat,Cu,Au\n"
         "A,0,0,10,1\n"
         "B,1,0,12,1.1\n"
         "C,1,1,40,3.5\n"
         "D,0,1,38,3.2\n"
         "E,0.5,0.5,15,1.4\n"
         "F,0.8,0.2,42,3.8\n";
  out.close();
  if (!out) {
    return false;
  }
  fix().csv = path;
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec) && !ec;
}

bool geochem_compute(void*) {
  fix().samples = gis::detail::load_geochem_csv(fix().csv);
  if (!fix().samples.ok || fix().samples.samples.size() != 6) {
    return false;
  }
  const auto stats =
      gis::detail::compute_geochem_stats(fix().samples, "Cu", 4, 2.0);
  return stats.ok && stats.threshold > stats.mean * 0.5;
}

bool geochem_command(void*) {
  const auto idw = gis::detail::run_geochem_idw(
      fix().samples, "Cu", 16, 2.0, std::numeric_limits<double>::quiet_NaN(),
      2.0);
  if (!idw.ok) {
    return false;
  }
  fix().raster =
      (std::filesystem::temp_directory_path() / "geochem_gate.tif")
          .generic_string();
  return gis::detail::write_geochem_idw_geotiff(fix().raster, idw, "");
}

bool geochem_visual(void*) {
  std::error_code ec;
  return !fix().raster.empty() &&
         std::filesystem::is_regular_file(fix().raster, ec) && !ec &&
         std::filesystem::file_size(fix().raster, ec) > 0 && !ec;
}

}  // namespace

ProductGateResult run_geochem_product_gate() {
  prepare_dll_search_path();
  clear_fix();
  static const ProductGateStep kSteps[] = {
      {"contract", geochem_contract},
      {"compute", geochem_compute},
      {"command", geochem_command},
      {"visual", geochem_visual},
  };
  const ProductGateResult result =
      run_product_gate("geochem", kSteps, 4, nullptr);
  clear_fix();
  return result;
}

}  // namespace plugin

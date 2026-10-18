// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/gate.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "gis/analysis/geology/borehole.h"
#include "gis/analysis/geology/prism_volume.h"
#include "gis/analysis/geology/stratum_tin.h"

namespace plugin {
namespace {

struct MineGateFix {
  std::string csv;
  std::string mesh;
  std::string volume;
  gis::detail::BoreholeSet holes;
};

MineGateFix& fix() {
  static MineGateFix state;
  return state;
}

std::string temp_file(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

void clear_fix() {
  std::error_code ec;
  if (!fix().csv.empty()) {
    std::filesystem::remove(fix().csv, ec);
  }
  if (!fix().mesh.empty()) {
    std::filesystem::remove(fix().mesh, ec);
  }
  if (!fix().volume.empty()) {
    std::filesystem::remove(fix().volume, ec);
  }
  fix() = {};
}

bool file_nonempty(const std::string& path) {
  std::error_code ec;
  return !path.empty() && std::filesystem::is_regular_file(path, ec) && !ec &&
         std::filesystem::file_size(path, ec) > 0 && !ec;
}

bool mine_contract(void*) {
  const std::string path = temp_file("mine_gate_boreholes.csv");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << "hole_id,x,y,z,stratum_id\n"
      << "BH1,116.35,39.90,50,clay\n"
      << "BH1,116.35,39.90,20,sand\n"
      << "BH2,116.40,39.92,48,clay\n"
      << "BH2,116.40,39.92,18,sand\n"
      << "BH3,116.38,39.88,52,clay\n"
      << "BH3,116.38,39.88,22,sand\n"
      << "BH4,116.36,39.91,49,clay\n"
      << "BH4,116.36,39.91,19,sand\n";
  out.close();
  if (!out) {
    return false;
  }
  fix().csv = path;
  return file_nonempty(fix().csv);
}

bool mine_compute(void*) {
  fix().holes = gis::detail::load_boreholes_csv(fix().csv);
  if (!fix().holes.ok || fix().holes.contacts.size() != 8) {
    return false;
  }
  const gis::detail::StratumTin tin =
      gis::detail::interpolate_stratum_tin(fix().holes, "clay");
  if (!tin.ok || tin.xyz.size() < 9 || tin.indices.size() < 3) {
    return false;
  }
  const gis::detail::PrismVolumeResult vol =
      gis::detail::prism_volume_between(fix().holes, "clay", "sand");
  return vol.ok && vol.volume > 0.0;
}

bool mine_command(void*) {
  fix().mesh = temp_file("mine_gate_mesh.json");
  fix().volume = temp_file("mine_gate_vol.json");
  const std::string interp = "{\"input\":\"" + fix().csv +
                             "\",\"stratum_id\":\"clay\",\"output\":\"" +
                             fix().mesh + "\"}";
  const std::string prism =
      "{\"input\":\"" + fix().csv +
      "\",\"top_stratum_id\":\"clay\",\"bottom_stratum_id\":\"sand\","
      "\"output\":\"" +
      fix().volume + "\"}";
  return gis::detail::run_stratum_interpolate_op(interp.c_str()) &&
         gis::detail::run_stratum_prism_volume_op(prism.c_str());
}

bool mine_visual(void*) {
  return file_nonempty(fix().mesh) && file_nonempty(fix().volume);
}

}  // namespace

ProductGateResult run_mine_product_gate() {
  clear_fix();
  static const ProductGateStep kSteps[] = {
      {"contract", mine_contract},
      {"compute", mine_compute},
      {"command", mine_command},
      {"visual", mine_visual},
  };
  const ProductGateResult result = run_product_gate("mine", kSteps, 4, nullptr);
  clear_fix();
  return result;
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "gis/analysis/geology/borehole.h"
#include "gis/analysis/geology/prism_volume.h"
#include "gis/analysis/geology/stratum_tin.h"

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

std::string write_toy_csv() {
  const std::string path = temp_file("mine_stratum_test_boreholes.csv");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  expect(static_cast<bool>(out), "open csv");
  if (!out) {
    return {};
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
  return path;
}

}  // namespace

int main() {
  const std::string csv = write_toy_csv();
  expect(!csv.empty(), "csv path");

  const gis::detail::BoreholeSet holes = gis::detail::load_boreholes_csv(csv);
  expect(holes.ok, "load ok");
  expect(holes.contacts.size() == 8, "contact count");

  const gis::detail::StratumTin tin =
      gis::detail::interpolate_stratum_tin(holes, "clay");
  expect(tin.ok, "tin ok");
  expect(tin.xyz.size() >= 9, "tin xyz");
  expect(tin.indices.size() >= 3, "tin indices");

  const gis::detail::PrismVolumeResult vol =
      gis::detail::prism_volume_between(holes, "clay", "sand");
  expect(vol.ok, "volume ok");
  expect(vol.volume > 0.0, "volume positive");

  const std::string mesh_out = temp_file("mine_stratum_test_mesh.json");
  expect(gis::detail::run_stratum_interpolate_op(
             ("{\"input\":\"" + csv +
              "\",\"stratum_id\":\"clay\",\"output\":\"" + mesh_out + "\"}")
                 .c_str()),
         "interpolate op");

  const std::string vol_out = temp_file("mine_stratum_test_vol.json");
  expect(gis::detail::run_stratum_prism_volume_op(
             ("{\"input\":\"" + csv +
              "\",\"top_stratum_id\":\"clay\",\"bottom_stratum_id\":\"sand\","
              "\"output\":\"" +
              vol_out + "\"}")
                 .c_str()),
         "prism op");

  std::error_code ec;
  std::filesystem::remove(csv, ec);
  std::filesystem::remove(mesh_out, ec);
  std::filesystem::remove(vol_out, ec);
  if (g_fails == 0) {
    std::printf("mine_stratum_test OK\n");
  }
  return g_fails == 0 ? 0 : 1;
}

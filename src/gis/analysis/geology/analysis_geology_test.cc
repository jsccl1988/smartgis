// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/borehole.h"
#include "gis/analysis/geology/prism_volume.h"
#include "gis/analysis/geology/stratum_tin.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool nearly(double a, double b, double tol) {
  return std::fabs(a - b) <= tol;
}

std::string write_temp_csv() {
  namespace fs = std::filesystem;
  const fs::path path = fs::temp_directory_path() / "smartgis_geology_bh.csv";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "hole_id,x,y,z,stratum_id\n"
         "BH1,0,0,100,ore_top\n"
         "BH1,0,0,90,ore_bottom\n"
         "BH2,10,0,102,ore_top\n"
         "BH2,10,0,91,ore_bottom\n"
         "BH3,10,10,101,ore_top\n"
         "BH3,10,10,89,ore_bottom\n"
         "BH4,0,10,99,ore_top\n"
         "BH4,0,10,88,ore_bottom\n";
  out.close();
  return path.string();
}

void test_load_and_tin() {
  const std::string path = write_temp_csv();
  const auto holes = gis::detail::load_boreholes_csv(path);
  expect(holes.ok, "load_boreholes_csv ok");
  expect(holes.contacts.size() == 8, "8 contacts");

  const auto tin =
      gis::detail::interpolate_stratum_tin(holes, "ore_top");
  expect(tin.ok, "interpolate_stratum_tin ok");
  expect(tin.xyz.size() == 12, "4 vertices * 3");
  expect(tin.indices.size() >= 6, "at least 2 triangles");
  expect(tin.stratum_id == "ore_top", "stratum_id");
}

void test_prism_volume_square() {
  const std::string path = write_temp_csv();
  const auto holes = gis::detail::load_boreholes_csv(path);
  expect(holes.ok, "load for prism");

  // Thicknesses: 10, 11, 12, 11 → avg 11.
  // Hull is unit square 10x10 → area 100 → volume 1100.
  const auto vol =
      gis::detail::prism_volume_between(holes, "ore_top", "ore_bottom");
  expect(vol.ok, "prism_volume ok");
  expect(nearly(vol.volume, 1100.0, 1e-6), "volume ~ 1100");
}

void test_ops_roundtrip() {
  const std::string csv = write_temp_csv();
  namespace fs = std::filesystem;
  const fs::path tin_out =
      fs::temp_directory_path() / "smartgis_geology_tin.json";
  const fs::path vol_out =
      fs::temp_directory_path() / "smartgis_geology_vol.json";

  // JSON strings need escaped backslashes on Windows paths.
  auto escape_json_path = [](const std::string& p) {
    std::string out;
    out.reserve(p.size() + 8);
    for (char c : p) {
      if (c == '\\' || c == '"') {
        out.push_back('\\');
      }
      out.push_back(c);
    }
    return out;
  };
  const std::string tin_args_esc =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"stratum_id\":\"ore_top\",\"output\":\"" +
      escape_json_path(tin_out.string()) + "\"}";
  expect(gis::detail::run_stratum_interpolate_op(tin_args_esc),
         "run_stratum_interpolate_op");

  const std::string vol_args_esc =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"top_stratum_id\":\"ore_top\",\"bottom_stratum_id\":\"ore_bottom\","
      "\"output\":\"" +
      escape_json_path(vol_out.string()) + "\"}";
  expect(gis::detail::run_stratum_prism_volume_op(vol_args_esc),
         "run_stratum_prism_volume_op");
}

}  // namespace

int main() {
  test_load_and_tin();
  test_prism_volume_square();
  test_ops_roundtrip();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("analysis_geology_test OK\n");
  return 0;
}

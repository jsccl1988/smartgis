// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geology/io/borehole_csv.h"
#include "gis/analysis/geology/kernel/prism_volume.h"
#include "gis/analysis/geology/kernel/stratum_tin.h"
#include "gis/analysis/geology/op/stratum_ops.h"

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

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

std::string escape_json_path(const std::string& p) {
  std::string out;
  out.reserve(p.size() + 8);
  for (char c : p) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

}  // namespace

TEST(AnalysisGeologyTest, LoadAndTin) {
  const std::string path = write_temp_csv();
  const auto holes = gis::load_boreholes_csv(path);
  ASSERT_TRUE(holes.ok) << holes.error;
  EXPECT_EQ(holes.contacts.size(), 8u);

  const auto tin = gis::interpolate_stratum_tin(holes, "ore_top");
  ASSERT_TRUE(tin.ok) << tin.error;
  EXPECT_EQ(tin.xyz.size(), 12u);
  EXPECT_GE(tin.indices.size(), 6u);
  EXPECT_EQ(tin.stratum_id, "ore_top");
}

TEST(AnalysisGeologyTest, PrismVolumeSquare) {
  const std::string path = write_temp_csv();
  const auto holes = gis::load_boreholes_csv(path);
  ASSERT_TRUE(holes.ok) << holes.error;

  // Thicknesses: 10, 11, 12, 11 → avg 11.
  // Hull is unit square 10x10 → area 100 → volume 1100.
  const auto vol =
      gis::prism_volume_between(holes, "ore_top", "ore_bottom");
  ASSERT_TRUE(vol.ok) << vol.error;
  EXPECT_NEAR(vol.volume, 1100.0, 1e-6);
}

TEST(AnalysisGeologyTest, OpsRoundtrip) {
  const std::string csv = write_temp_csv();
  namespace fs = std::filesystem;
  const fs::path tin_out =
      fs::temp_directory_path() / "smartgis_geology_tin.json";
  const fs::path vol_out =
      fs::temp_directory_path() / "smartgis_geology_vol.json";

  const std::string tin_args =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"stratum_id\":\"ore_top\",\"output\":\"" +
      escape_json_path(tin_out.string()) + "\"}";
  ASSERT_TRUE(gis::run_stratum_interpolate_op(tin_args));

  const std::string vol_args =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"top_stratum_id\":\"ore_top\",\"bottom_stratum_id\":\"ore_bottom\","
      "\"output\":\"" +
      escape_json_path(vol_out.string()) + "\"}";
  ASSERT_TRUE(gis::run_stratum_prism_volume_op(vol_args));
}

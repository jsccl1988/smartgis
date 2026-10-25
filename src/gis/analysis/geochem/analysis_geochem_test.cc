// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/grade/grade.h"
#include "gis/analysis/geochem/idw/idw.h"
#include "gis/analysis/geochem/load/csv_load.h"
#include "gis/analysis/geochem/op/idw_op.h"
#include "gis/analysis/geochem/op/stats_op.h"
#include "gis/analysis/geochem/stats/stats.h"
#include "gis/analysis/ops/runner/ops_runner.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace {

std::string write_temp_csv() {
  namespace fs = std::filesystem;
  const fs::path path =
      fs::temp_directory_path() / "smartgis_geochem_samples.csv";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "id,lon,lat,Cu,Au,Pb\n"
         "S1,114.20,30.50,12,0.4,8\n"
         "S2,114.25,30.52,18,0.6,11\n"
         "S3,114.30,30.48,45,1.8,22\n"
         "S4,114.28,30.55,52,2.1,28\n"
         "S5,114.22,30.53,15,0.5,9\n"
         "S6,114.35,30.50,48,1.9,25\n"
         "S7,114.32,30.54,55,2.4,30\n"
         "S8,114.18,30.48,10,0.3,7\n"
         "S9,114.27,30.49,40,1.5,20\n"
         "S10,114.24,30.56,14,0.45,10\n";
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

TEST(GeochemAnalysis, LoadStatsIdw) {
  const std::string csv = write_temp_csv();
  const auto set = gis::detail::load_csv(csv);
  ASSERT_TRUE(set.ok);
  EXPECT_EQ(set.samples.size(), 10u);
  EXPECT_EQ(set.element_names.size(), 3u);

  const auto st = gis::detail::compute_stats(set, "Cu", 5, 2.0);
  ASSERT_TRUE(st.ok);
  EXPECT_EQ(st.count, 10);
  EXPECT_GT(st.threshold, st.background);
  EXPECT_EQ(st.histogram.size(), 5u);

  const auto corr = gis::detail::compute_correlation(set, "Cu", "Au");
  ASSERT_TRUE(corr.ok);
  EXPECT_GT(corr.r, 0.9);

  const auto legend = gis::detail::build_grade_legend(set, "Cu", 5);
  ASSERT_TRUE(legend.ok);
  EXPECT_EQ(legend.classes.size(), 5u);

  const auto idw = gis::detail::run_idw(
      set, "Cu", 32, 2.0, std::numeric_limits<double>::quiet_NaN(), 2.0);
  ASSERT_TRUE(idw.ok);
  EXPECT_GT(idw.width, 0);
  EXPECT_GT(idw.height, 0);
  EXPECT_FALSE(idw.values.empty());
  EXPECT_EQ(idw.values.size(),
            static_cast<size_t>(idw.width) * static_cast<size_t>(idw.height));
  EXPECT_FALSE(idw.anomaly_mask.empty());

  double vmin = 0;
  double vmax = 0;
  bool range_set = false;
  for (float v : idw.values) {
    if (!std::isfinite(v) || v <= -9998.5f) {
      continue;
    }
    if (!range_set) {
      vmin = vmax = static_cast<double>(v);
      range_set = true;
    } else {
      vmin = std::min(vmin, static_cast<double>(v));
      vmax = std::max(vmax, static_cast<double>(v));
    }
  }
  ASSERT_TRUE(range_set);
  EXPECT_GE(vmax, vmin);
  const double mid = 0.5 * (vmin + vmax);
  const double score = gis::detail::heat_score(mid, vmin, vmax);
  EXPECT_NEAR(score, 50.0, 1.0);
  char heat_buf[32];
  EXPECT_NE(gis::detail::format_heat(score, heat_buf, sizeof(heat_buf))[0],
            '\0');
}

TEST(GeochemAnalysis, OpsRoundtrip) {
  const std::string csv = write_temp_csv();
  namespace fs = std::filesystem;
  const fs::path stats_out =
      fs::temp_directory_path() / "smartgis_geochem_stats.json";
  const fs::path idw_out =
      fs::temp_directory_path() / "smartgis_geochem_idw.tif";

  const std::string stats_args =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"element\":\"Cu\",\"bins\":5,\"k_sigma\":2,\"correlate\":\"Au\","
      "\"output\":\"" +
      escape_json_path(stats_out.string()) + "\"}";
  ASSERT_TRUE(gis::detail::run_stats_op(stats_args));
  ASSERT_TRUE(gis::detail::run_builtin_op("native.geochem_stats", stats_args));

  const std::string idw_args =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"element\":\"Cu\",\"cells\":24,\"output\":\"" +
      escape_json_path(idw_out.string()) + "\"}";
  ASSERT_TRUE(gis::detail::run_idw_op(idw_args));
  ASSERT_TRUE(gis::detail::run_builtin_op("native.geochem_idw", idw_args));
}

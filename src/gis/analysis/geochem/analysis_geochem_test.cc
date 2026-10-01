// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/grade.h"
#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/samples.h"
#include "gis/analysis/geochem/stats.h"
#include "gis/analysis/ops/ops_runner.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
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

void test_load_stats_idw() {
  const std::string csv = write_temp_csv();
  const auto set = gis::detail::load_geochem_csv(csv);
  expect(set.ok, "load_geochem_csv ok");
  expect(set.samples.size() == 10, "10 samples");
  expect(set.element_names.size() == 3, "3 elements");

  const auto st = gis::detail::compute_geochem_stats(set, "Cu", 5, 2.0);
  expect(st.ok, "stats ok");
  expect(st.count == 10, "stats count");
  expect(st.threshold > st.background, "threshold > background");
  expect(st.histogram.size() == 5, "5 bins");

  const auto corr =
      gis::detail::compute_geochem_correlation(set, "Cu", "Au");
  expect(corr.ok, "corr ok");
  expect(corr.r > 0.9, "Cu-Au highly correlated");

  const auto legend =
      gis::detail::build_geochem_grade_legend(set, "Cu", 5);
  expect(legend.ok, "legend ok");
  expect(legend.classes.size() == 5, "5 classes");

  const auto idw =
      gis::detail::run_geochem_idw(set, "Cu", 32, 2.0,
                                  std::numeric_limits<double>::quiet_NaN(),
                                  2.0);
  expect(idw.ok, "idw ok");
  expect(idw.width > 0 && idw.height > 0, "idw size");
  expect(!idw.values.empty(), "idw values");
  expect(idw.values.size() ==
             static_cast<size_t>(idw.width) * static_cast<size_t>(idw.height),
         "full-extent raster");
  expect(!idw.anomaly_mask.empty(), "anomaly mask");
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
  expect(range_set && vmax >= vmin, "idw value range");
  const double mid = 0.5 * (vmin + vmax);
  const double score = gis::detail::geochem_heat_score(mid, vmin, vmax);
  expect(nearly(score, 50.0, 1.0), "mid heat ~50");
  char heat_buf[32];
  expect(gis::detail::format_geochem_heat(score, heat_buf, sizeof(heat_buf))[0] !=
             '\0',
         "format heat");
}

void test_ops_roundtrip() {
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
  expect(gis::detail::run_geochem_stats_op(stats_args), "stats op");
  expect(gis::detail::run_builtin_op("native.geochem_stats", stats_args),
         "catalog stats");

  const std::string idw_args =
      std::string("{\"input\":\"") + escape_json_path(csv) +
      "\",\"element\":\"Cu\",\"cells\":24,\"output\":\"" +
      escape_json_path(idw_out.string()) + "\"}";
  expect(gis::detail::run_geochem_idw_op(idw_args), "idw op");
  expect(gis::detail::run_builtin_op("native.geochem_idw", idw_args),
         "catalog idw");
}

}  // namespace

int main() {
  test_load_stats_idw();
  test_ops_roundtrip();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("analysis_geochem_test OK\n");
  return 0;
}

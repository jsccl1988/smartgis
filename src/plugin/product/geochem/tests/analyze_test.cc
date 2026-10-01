// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/samples.h"
#include "gis/analysis/geochem/stats.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string write_toy_csv() {
  const std::string path =
      (std::filesystem::temp_directory_path() / "geochem_analyze_test.csv")
          .generic_string();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "id,lon,lat,Cu,Au\n"
         "A,0,0,10,1\n"
         "B,1,0,12,1.1\n"
         "C,1,1,40,3.5\n"
         "D,0,1,38,3.2\n"
         "E,0.5,0.5,15,1.4\n"
         "F,0.8,0.2,42,3.8\n";
  out.close();
  return path;
}

}  // namespace

int main() {
  const std::string csv = write_toy_csv();
  const auto set = gis::detail::load_geochem_csv(csv);
  expect(set.ok, "load csv");
  expect(set.samples.size() == 6, "6 samples");

  const auto st = gis::detail::compute_geochem_stats(set, "Cu", 4, 2.0);
  expect(st.ok, "stats");
  expect(st.threshold > st.mean * 0.5, "threshold sane");

  const auto idw = gis::detail::run_geochem_idw(
      set, "Cu", 16, 2.0, std::numeric_limits<double>::quiet_NaN(), 2.0);
  expect(idw.ok, "idw");
  const std::string out =
      (std::filesystem::temp_directory_path() / "geochem_analyze_test.tif")
          .generic_string();
  expect(gis::detail::write_geochem_idw_geotiff(out, idw, ""), "write tif");

  std::error_code ec;
  std::filesystem::remove(csv, ec);
  std::filesystem::remove(out, ec);
  if (g_fails == 0) {
    std::printf("geochem_analyze_test OK\n");
  }
  return g_fails == 0 ? 0 : 1;
}

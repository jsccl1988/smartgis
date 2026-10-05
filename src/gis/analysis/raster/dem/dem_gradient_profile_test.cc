// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/dem/dem_gradient_profile.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::vector<float> make_ramp(int w, int h) {
  std::vector<float> elev(static_cast<size_t>(w) * static_cast<size_t>(h));
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      elev[static_cast<size_t>(r) * static_cast<size_t>(w) +
           static_cast<size_t>(c)] = static_cast<float>(c);
    }
  }
  return elev;
}

void test_small_grid_serial() {
  const int w = 4;
  const int h = 4;
  const auto elev = make_ramp(w, h);
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchAuto);
  const auto g = gis::detail::compute_dem_gradient(elev, w, h, 1.0, 1.0);
  expect(g.ok, "4x4 ok");
  const float s = g.slope_deg[static_cast<size_t>(1 * w + 1)];
  expect(std::fabs(static_cast<double>(s) - 45.0) <= 1.0, "4x4 slope ~ 45");
  const auto p = gis::detail::last_dem_gradient_profile();
  expect(std::strcmp(p.backend, "serial") == 0, "4x4 backend serial");
  expect(p.threads == 1, "4x4 threads=1");
  expect(p.pixels == 16, "4x4 pixels");
}

void test_threshold_parallel() {
  const int n = 64;
  const auto elev = make_ramp(n, n);
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchAuto);
  const auto g = gis::detail::compute_dem_gradient(elev, n, n, 1.0, 1.0);
  expect(g.ok, "64x64 ok");
  const auto p = gis::detail::last_dem_gradient_profile();
  expect(std::strcmp(p.backend, "parallel") == 0, "64x64 backend parallel");
  expect(p.threads == gis::detail::kDemGradientPoolThreads, "64x64 pool threads");
}

void test_short_rows_stay_serial() {
  const int w = 1024;
  const int h = 7;
  const auto elev = make_ramp(w, h);
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchAuto);
  const auto g = gis::detail::compute_dem_gradient(elev, w, h, 1.0, 1.0);
  expect(g.ok, "7x1024 ok");
  const auto p = gis::detail::last_dem_gradient_profile();
  expect(std::strcmp(p.backend, "serial") == 0, "rows<8 serial despite pixels");
}

void test_force_serial_matches_parallel() {
  const int n = 32;
  const auto elev = make_ramp(n, n);
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchSerial);
  const auto serial = gis::detail::compute_dem_gradient(elev, n, n, 1.0, 1.0);
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchParallel);
  const auto parallel = gis::detail::compute_dem_gradient(elev, n, n, 1.0, 1.0);
  gis::detail::set_dem_gradient_dispatch_override(
      gis::detail::kGradientDispatchAuto);
  expect(serial.ok && parallel.ok, "forced serial/parallel ok");
  expect(serial.slope_deg.size() == parallel.slope_deg.size(), "slope size");
  bool match = true;
  for (size_t i = 0; i < serial.slope_deg.size(); ++i) {
    if (std::fabs(serial.slope_deg[i] - parallel.slope_deg[i]) > 1e-5f ||
        std::fabs(serial.aspect_deg[i] - parallel.aspect_deg[i]) > 1e-5f) {
      match = false;
      break;
    }
  }
  expect(match, "forced serial equals parallel");
}

void test_profile_dump() {
  const auto dump = std::filesystem::current_path() / "log" /
                    "dem_gradient_profile_test.json";
  std::filesystem::create_directories(dump.parent_path());
#if defined(_WIN32)
  _putenv_s("ANALYSIS_PROFILE", "1");
  _putenv_s("ANALYSIS_PROFILE_DUMP", dump.string().c_str());
#else
  setenv("ANALYSIS_PROFILE", "1", 1);
  setenv("ANALYSIS_PROFILE_DUMP", dump.string().c_str(), 1);
#endif
  expect(gis::detail::dem_gradient_profile_enabled(), "profile env on");
  const auto elev = make_ramp(8, 8);
  (void)gis::detail::compute_dem_gradient(elev, 8, 8, 1.0, 1.0);
  expect(std::filesystem::exists(dump),
         "profile json dumped (ANALYSIS_PROFILE_DUMP)");
#if defined(_WIN32)
  _putenv_s("ANALYSIS_PROFILE", "0");
  _putenv_s("ANALYSIS_PROFILE_DUMP", "");
#else
  unsetenv("ANALYSIS_PROFILE");
  unsetenv("ANALYSIS_PROFILE_DUMP");
#endif
}

}  // namespace

int main() {
  test_small_grid_serial();
  test_threshold_parallel();
  test_short_rows_stay_serial();
  test_force_serial_matches_parallel();
  test_profile_dump();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("analysis_dem_gradient_profile_test OK\n");
  return 0;
}

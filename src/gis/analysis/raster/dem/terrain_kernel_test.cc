// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/hillshade.h"
#include "gis/analysis/raster/mask/ring_mask.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_flat_lambert_is_sin_altitude() {
  constexpr float kAlt = 0.52359878f;  // 30 deg
  const float shade = gis::detail::horn_lambert_shade(
      100.f, 100.f, 100.f, 100.f, 2.f, 2.f, 1.f, 0.f, std::sin(kAlt),
      std::cos(kAlt));
  expect(std::fabs(shade - std::sin(kAlt)) < 1e-5f, "flat shade == sin(alt)");
}

void test_east_rise_matches_horn() {
  // alt = 0, az = 0: shade = sin(slope) * cos(az - aspect), slope = atan(1).
  const float shade = gis::detail::horn_lambert_shade(0.f, 2.f, 0.f, 0.f, 2.f,
                                                     2.f, 1.f, 0.f, 0.f, 1.f);
  const float expect_shade = std::sin(std::atan(1.f));
  expect(std::fabs(shade - expect_shade) < 1e-5f, "east rise lambert");
}

void test_grid_matches_scalar() {
  const float heights[] = {
      0.f, 0.f, 0.f, 1.f, 2.f, 3.f, 2.f, 2.f, 2.f,
  };
  std::vector<float> shade;
  int w = 0;
  int h = 0;
  expect(gis::detail::horn_lambert_shade_grid(heights, 3, 3, 1, 1, 2.f, 2.f, 1.f,
                                             0.f, 0.f, 1.f, &shade, &w, &h),
         "grid shade");
  expect(w == 3 && h == 3 && shade.size() == 9, "grid size");
  const float mid = gis::detail::horn_lambert_shade(0.f, 2.f, 2.f, 0.f, 2.f, 2.f,
                                                   1.f, 0.f, 0.f, 1.f);
  expect(std::fabs(shade[1 * 3 + 1] - mid) < 1e-5f, "grid center matches scalar");
}

void test_point_in_unit_square() {
  const double x[] = {0.0, 1.0, 1.0, 0.0};
  const double y[] = {0.0, 0.0, 1.0, 1.0};
  expect(gis::detail::point_in_ring(0.5, 0.5, x, y, 4), "center inside");
  expect(!gis::detail::point_in_ring(1.5, 0.5, x, y, 4), "outside");
  expect(!gis::detail::point_in_ring(0.5, 0.5, x, y, 2), "short ring outside");
}

void test_fill_ring_mask_center() {
  const double x[] = {0.0, 1.0, 1.0, 0.0};
  const double y[] = {0.0, 0.0, 1.0, 1.0};
  const int off[] = {0, 4};
  gis::detail::RingMaskBBox box{0.0, 0.0, 1.0, 1.0, true};
  uint8_t mask[9] = {};
  gis::detail::fill_ring_mask(0.0, 0.0, 1.0, 1.0, 3, 3, x, y, off, 1, &box,
                             mask);
  // cols: lon 0, 0.5, 1. rows: lat 1, 0.5, 0. Center is (0.5, 0.5).
  expect(mask[1 * 3 + 1] == 1, "mask center inside");
  expect(mask[0 * 3 + 2] == 0, "mask north-east outside");
}

}  // namespace

int main() {
  test_flat_lambert_is_sin_altitude();
  test_east_rise_matches_horn();
  test_grid_matches_scalar();
  test_point_in_unit_square();
  test_fill_ring_mask_center();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("analysis_terrain_kernel_test OK\n");
  return 0;
}

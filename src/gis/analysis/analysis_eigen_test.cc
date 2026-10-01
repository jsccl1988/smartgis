// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geometry/fit.h"
#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/filter/raster_convolve.h"
#include "gis/analysis/raster/filter/raster_smooth.h"

#include <cmath>
#include <cstdint>
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

bool nearly(double a, double b, double tol) {
  return std::fabs(a - b) <= tol;
}

void test_fit_line() {
  // Points on y = 2x + 1 with small noise.
  const std::vector<double> xy = {
      0.0, 1.0, 1.0, 3.0, 2.0, 5.0, 3.0, 7.0, 4.0, 9.0,
  };
  const auto fit = gis::detail::fit_line_2d(xy);
  expect(fit.ok, "fit_line ok");
  const double dir_dot =
      std::fabs(fit.dx * (1.0 / std::sqrt(5.0)) + fit.dy * (2.0 / std::sqrt(5.0)));
  expect(dir_dot > 0.99, "fit_line direction ~ (1,2)");
  expect(fit.rms < 1e-9, "fit_line rms near zero");
}

void test_fit_plane() {
  // z = 0 plane (xy points).
  const std::vector<double> xyz = {
      0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0,
  };
  const auto fit = gis::detail::fit_plane_3d(xyz);
  expect(fit.ok, "fit_plane ok");
  expect(nearly(std::fabs(fit.c), 1.0, 1e-6), "fit_plane normal ~ Z");
  expect(fit.rms < 1e-9, "fit_plane rms");
}

void test_affine_align() {
  // Identity + translation (2, 3).
  const std::vector<double> src = {0, 0, 1, 0, 0, 1, 2, 2};
  const std::vector<double> dst = {2, 3, 3, 3, 2, 4, 4, 5};
  const auto fit = gis::detail::affine_align_2d(src, dst);
  expect(fit.ok, "affine ok");
  expect(nearly(fit.m[0], 1.0, 1e-9) && nearly(fit.m[4], 1.0, 1e-9),
         "affine linear part I");
  expect(nearly(fit.m[2], 2.0, 1e-9) && nearly(fit.m[5], 3.0, 1e-9),
         "affine translation");
  expect(fit.rms < 1e-9, "affine rms");
}

void test_dem_gradient_ramp() {
  // 4x4 ramp in x: elev = col.
  const int w = 4;
  const int h = 4;
  std::vector<float> elev(static_cast<size_t>(w * h));
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      elev[static_cast<size_t>(r * w + c)] = static_cast<float>(c);
    }
  }
  const auto g = gis::detail::compute_dem_gradient(elev, w, h, 1.0, 1.0);
  expect(g.ok, "gradient ok");
  // Interior slope ~ atan(1) ~ 45 deg.
  const float s = g.slope_deg[static_cast<size_t>(1 * w + 1)];
  expect(nearly(static_cast<double>(s), 45.0, 1.0), "slope ~ 45");
}

void test_convolve_box3_constant() {
  const int w = 5;
  const int h = 5;
  std::vector<float> elev(static_cast<size_t>(w * h), 7.f);
  const auto r = gis::detail::convolve_box3(elev, w, h);
  expect(r.ok, "convolve ok");
  expect(nearly(static_cast<double>(r.values[12]), 7.0, 1e-5),
         "box mean of constant");
}

void test_smooth_laplace_interior() {
  const int w = 5;
  const int h = 5;
  std::vector<float> values(static_cast<size_t>(w * h), 0.f);
  std::vector<std::uint8_t> unk(static_cast<size_t>(w * h), 0);
  // Boundary Dirichlet: left=0, right=10, top/bottom linear.
  for (int r = 0; r < h; ++r) {
    for (int c = 0; c < w; ++c) {
      const size_t k = static_cast<size_t>(r * w + c);
      if (c == 0 || c == w - 1 || r == 0 || r == h - 1) {
        values[k] = static_cast<float>(c) * 2.5f;  // 0..10
        unk[k] = 0;
      } else {
        values[k] = 99.f;
        unk[k] = 1;
      }
    }
  }
  const auto s = gis::detail::smooth_raster_laplace(values, w, h, unk.data());
  expect(s.ok, "smooth ok");
  // Harmonic on linear boundary → linear field: center ~ 5.
  expect(nearly(static_cast<double>(s.values[static_cast<size_t>(2 * w + 2)]),
                5.0, 0.25),
         "smooth center ~ 5");
}

}  // namespace

int main() {
  test_fit_line();
  test_fit_plane();
  test_affine_align();
  test_dem_gradient_ramp();
  test_convolve_box3_constant();
  test_smooth_laplace_interior();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("analysis_eigen_test OK\n");
  return 0;
}

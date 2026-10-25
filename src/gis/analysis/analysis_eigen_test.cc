// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geometry/affine_construct.h"
#include "gis/analysis/geometry/fit.h"
#include "gis/analysis/raster/dem/dem_gradient.h"
#include "gis/analysis/raster/filter/raster_convolve.h"
#include "gis/analysis/raster/filter/raster_smooth.h"

#include "ogr_geometry.h"

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

void test_fit_line_ogr() {
  OGRLineString line;
  line.addPoint(0.0, 1.0);
  line.addPoint(1.0, 3.0);
  line.addPoint(2.0, 5.0);
  line.addPoint(3.0, 7.0);
  line.addPoint(4.0, 9.0);
  static_assert(geo::ogr_geometry_like<OGRLineString>);
  const auto fit = gis::detail::fit_line_2d(line);
  expect(fit.ok, "fit_line ogr ok");
  expect(fit.rms < 1e-9, "fit_line ogr rms");
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

void test_similarity_align() {
  // 90° CCW + scale 2 about origin: (1,0) -> (0,2).
  const std::vector<double> src = {1, 0, 0, 1, 1, 1};
  const std::vector<double> dst = {0, 2, -2, 0, 0, 2};
  const auto fit = gis::detail::similarity_align_2d(src, dst);
  expect(fit.ok, "similarity ok");
  expect(nearly(fit.scale, 2.0, 1e-6), "similarity scale ~ 2");
  expect(nearly(fit.m[0] * 1 + fit.m[1] * 0 + fit.m[2], 0.0, 1e-6),
         "similarity maps (1,0)->(0,2) x");
  expect(nearly(fit.m[3] * 1 + fit.m[4] * 0 + fit.m[5], 2.0, 1e-6),
         "similarity maps (1,0)->(0,2) y");
  expect(fit.rms < 1e-6, "similarity rms");
}

void test_rigid_align() {
  const std::vector<double> src = {1, 0, 0, 0};
  const std::vector<double> dst = {0, 1, -1, 0};
  const auto fit = gis::detail::rigid_align_2d(src, dst);
  expect(fit.ok, "rigid ok");
  expect(nearly(fit.scale, 1.0, 1e-6), "rigid unit scale");
  expect(fit.rms < 1e-6, "rigid rms");
}

void test_pca_axes() {
  const std::vector<double> xy = {
      -2, 0, -1, 0, 0, 0, 1, 0, 2, 0, 0, 0.1, 0, -0.1,
  };
  const auto pca = gis::detail::pca_axes_2d(xy);
  expect(pca.ok, "pca ok");
  expect(std::fabs(pca.major_dx) > 0.99, "pca major ~ X");
  expect(pca.major_extent > 3.5, "pca major extent");
  expect(pca.minor_extent < 0.5, "pca minor extent small");
}

void test_oriented_bbox() {
  const std::vector<double> xy = {
      0, 0, 4, 0, 4, 2, 0, 2,
  };
  auto box = gis::detail::oriented_bbox_from_points_2d(xy);
  expect(box != nullptr && !box->IsEmpty(), "oriented bbox ok");
  expect(wkbFlatten(box->getGeometryType()) == wkbPolygon, "oriented bbox polygon");
}

void test_affine_construct() {
  OGRPoint pt(1.0, 2.0);
  expect(gis::detail::affine_translate_2d(&pt, 3.0, -1.0), "translate ok");
  expect(nearly(pt.getX(), 4.0, 1e-9) && nearly(pt.getY(), 1.0, 1e-9),
         "translate coords");
  OGRPoint pivot(0.0, 0.0);
  expect(gis::detail::affine_rotate_2d(&pivot, 1.5707963267948966, 0.0, 0.0),
         "rotate ok");
  expect(nearly(pivot.getX(), 0.0, 1e-6) && nearly(pivot.getY(), 0.0, 1e-6),
         "origin fixed under rotate");
  OGRPoint p2(1.0, 0.0);
  expect(gis::detail::affine_rotate_2d(&p2, 1.5707963267948966, 0.0, 0.0),
         "rotate p2 ok");
  expect(nearly(p2.getX(), 0.0, 1e-6) && nearly(p2.getY(), 1.0, 1e-6),
         "rotate 90 CCW");
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
  test_fit_line_ogr();
  test_fit_plane();
  test_affine_align();
  test_similarity_align();
  test_rigid_align();
  test_pca_axes();
  test_oriented_bbox();
  test_affine_construct();
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

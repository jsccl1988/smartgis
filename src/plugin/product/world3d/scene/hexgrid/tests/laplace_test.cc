// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/hexgrid/solve/boundary_solve.h"
#include "plugin/product/world3d/scene/hexgrid/sample/sample_volume.h"
#include "plugin/product/world3d/scene/hexgrid/io/vtk_structured.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_unit_box_laplace() {
  plugin::detail::Xyz corners[8] = {
      {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
  };
  const plugin::detail::HexCornerSolve solved =
      plugin::detail::solve_hex_from_corners(corners, 5, 5, 5);
  expect(solved.ok, "unit box solve ok");
  expect(solved.grid.nx == 5 && solved.grid.ny == 5 && solved.grid.nz == 5,
         "dims");
  double mid_x = 0;
  double mid_y = 0;
  double mid_z = 0;
  expect(solved.grid.hex_point(2, 2, 2, &mid_x, &mid_y, &mid_z), "mid node");
  expect(std::fabs(mid_x - 0.5) < 1e-9 && std::fabs(mid_y - 0.5) < 1e-9 &&
             std::fabs(mid_z - 0.5) < 1e-9,
         "center at 0.5");
  expect(!solved.cell_orth.empty(), "has cell orth");
  float max_skew = 0.f;
  for (float s : solved.cell_orth) {
    if (s > max_skew) {
      max_skew = s;
    }
  }
  expect(max_skew < 1e-3f, "unit box nearly orthogonal");
}

void test_warped_and_vts() {
  plugin::detail::Xyz corners[8] = {
      {0, 0, 0}, {2, 0, 0}, {2.2, 1.5, 0}, {0, 1, 0},
      {0, 0, 1}, {2, 0, 1.1}, {2.1, 1.4, 1.2}, {-0.1, 1, 1},
  };
  const plugin::detail::HexCornerSolve solved =
      plugin::detail::solve_hex_from_corners(corners, 4, 4, 4);
  expect(solved.ok, "warped solve ok");

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "orthogrid3d_test.vts";
  const bool wrote = plugin::detail::write_vtk_structured(
      solved.grid, path.string(), solved.cell_orth.data(),
      solved.cell_orth.size());
  expect(wrote, "vts write");
  expect(std::filesystem::file_size(path) > 100, "vts non-empty");
  std::filesystem::remove(path);
}

void test_quarry_tin_sample() {
  const plugin::detail::HexCornerSolve solved =
      plugin::detail::solve_hex_from_quarry_sample(
          plugin::detail::k_demo_nx, plugin::detail::k_demo_ny,
          plugin::detail::k_demo_nz);
  expect(solved.ok, "quarry tin solve ok");
  expect(solved.grid.nx == plugin::detail::k_demo_nx &&
             solved.grid.ny == plugin::detail::k_demo_ny &&
             solved.grid.nz == plugin::detail::k_demo_nz,
         "demo dims");
  const int cells = (plugin::detail::k_demo_nx - 1) *
                    (plugin::detail::k_demo_ny - 1) *
                    (plugin::detail::k_demo_nz - 1);
  expect(static_cast<int>(solved.cell_orth.size()) == cells,
         "many hex cells");
  expect(cells > 1000, "not a toy 2x2 lattice");

  double zmin = 1e300;
  double zmax = -1e300;
  double xmin = 1e300;
  double xmax = -1e300;
  for (int k = 0; k < solved.grid.nz; ++k) {
    for (int j = 0; j < solved.grid.ny; ++j) {
      for (int i = 0; i < solved.grid.nx; ++i) {
        double x = 0;
        double y = 0;
        double z = 0;
        expect(solved.grid.hex_point(i, j, k, &x, &y, &z), "quarry node");
        zmin = std::min(zmin, z);
        zmax = std::max(zmax, z);
        xmin = std::min(xmin, x);
        xmax = std::max(xmax, x);
      }
    }
  }
  expect(xmax - xmin > 1000.0, "projected-meter easting span");
  expect(zmax - zmin > 40.0, "mixed elevations");
  expect(zmin > 50.0 && zmax < 400.0, "terrain-like Z band");
  expect(solved.message.find("quarry_tin") != std::string::npos,
         "sample tag in result");
}

}  // namespace

int main() {
  test_unit_box_laplace();
  test_warped_and_vts();
  test_quarry_tin_sample();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("orthogrid3d_laplace_test OK\n");
  return 0;
}

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid3d/detail/boundary_solve.h"
#include "plugin/product/orthogrid3d/detail/laplace_solver.h"
#include "plugin/product/orthogrid3d/detail/orthogonality.h"
#include "plugin/product/orthogrid3d/detail/vtk_structured.h"

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
  geo::Raw3DPoint corners[8] = {
      {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
  };
  const plugin::detail::HexCornerSolve solved =
      plugin::detail::solve_hex_from_corners(corners, 5, 5, 5);
  expect(solved.ok, "unit box solve ok");
  expect(solved.grid.nx() == 5 && solved.grid.ny() == 5 &&
             solved.grid.nz() == 5,
         "dims");
  const geo::Raw3DPoint mid = solved.grid.node(2, 2, 2);
  expect(std::fabs(mid.x - 0.5) < 1e-9 && std::fabs(mid.y - 0.5) < 1e-9 &&
             std::fabs(mid.z - 0.5) < 1e-9,
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
  geo::Raw3DPoint corners[8] = {
      {0, 0, 0}, {2, 0, 0}, {2.2, 1.5, 0}, {0, 1, 0},
      {0, 0, 1}, {2, 0, 1.1}, {2.1, 1.4, 1.2}, {-0.1, 1, 1},
  };
  const plugin::detail::HexCornerSolve solved =
      plugin::detail::solve_hex_from_corners(corners, 4, 4, 4);
  expect(solved.ok, "warped solve ok");

  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "orthogrid3d_test.vts";
  const bool wrote = orthogrid3d::write_vtk_structured(
      solved.grid, path.string(), solved.cell_orth.data(),
      solved.cell_orth.size());
  expect(wrote, "vts write");
  expect(std::filesystem::file_size(path) > 100, "vts non-empty");
  std::filesystem::remove(path);
}

}  // namespace

int main() {
  test_unit_box_laplace();
  test_warped_and_vts();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("orthogrid3d_laplace_test OK\n");
  return 0;
}

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid/detail/boundary_solve.h"
#include "plugin/product/orthogrid/detail/laplace_solver.h"
#include "plugin/product/orthogrid/detail/orthogonality.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
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

int idx(int nx, int i, int j) {
  return j * nx + i;
}

void fill_unit_square_scrambled(int nx,
                                int ny,
                                orthogrid::GridField* grid,
                                std::vector<std::uint8_t>* unknown) {
  unknown->assign(static_cast<size_t>(nx * ny), 0);
  std::vector<double> xs(static_cast<size_t>(nx * ny), 0.0);
  std::vector<double> ys(static_cast<size_t>(nx * ny), 0.0);
  const double dx = 1.0 / static_cast<double>(nx - 1);
  const double dy = 1.0 / static_cast<double>(ny - 1);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int k = idx(nx, i, j);
      const bool interior = (i > 0 && i < nx - 1 && j > 0 && j < ny - 1);
      (*unknown)[static_cast<size_t>(k)] = interior ? 1 : 0;
      if (interior) {
        xs[static_cast<size_t>(k)] = 0.37;
        ys[static_cast<size_t>(k)] = 0.63;
      } else {
        xs[static_cast<size_t>(k)] = static_cast<double>(i) * dx;
        ys[static_cast<size_t>(k)] = static_cast<double>(j) * dy;
      }
    }
  }
  grid->assign_from_flat(nx, ny, xs.data(), ys.data());
}

bool nearly(double a, double b, double tol) {
  return std::fabs(a - b) <= tol;
}

void test_rejects_tiny_grid() {
  orthogrid::GridField g;
  g.nx = 2;
  g.ny = 2;
  g.x = orthogrid::GridArray::Zero(2, 2);
  g.y = orthogrid::GridArray::Zero(2, 2);
  std::uint8_t unk = 1;
  expect(!orthogrid::solve_laplace(g, &unk), "tiny grid should fail");
}

void test_no_unknowns_is_noop() {
  const int nx = 3;
  const int ny = 3;
  std::vector<double> xs(9);
  std::vector<double> ys(9);
  std::vector<std::uint8_t> unknown(9, 0);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      xs[static_cast<size_t>(idx(nx, i, j))] = static_cast<double>(i);
      ys[static_cast<size_t>(idx(nx, i, j))] = static_cast<double>(j) + 2.0;
    }
  }
  orthogrid::GridField g;
  g.assign_from_flat(nx, ny, xs.data(), ys.data());
  expect(orthogrid::solve_laplace(g, unknown.data()), "all-Dirichlet ok");
  expect(nearly(g.x(1, 1), 1.0, 1e-12), "Dirichlet x unchanged");
  expect(nearly(g.y(1, 1), 3.0, 1e-12), "Dirichlet y unchanged");
}

void test_unit_square_recovers_bilinear() {
  const int nx = 5;
  const int ny = 5;
  orthogrid::GridField g;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &g, &unknown);
  expect(orthogrid::solve_laplace(g, unknown.data()), "laplace solve ok");
  const double dx = 0.25;
  const double dy = 0.25;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      if (!nearly(g.x(j, i), static_cast<double>(i) * dx, 1e-9) ||
          !nearly(g.y(j, i), static_cast<double>(j) * dy, 1e-9)) {
        std::fprintf(stderr, "FAIL: node (%d,%d) got (%g,%g)\n", i, j, g.x(j, i),
                     g.y(j, i));
        ++g_fails;
      }
    }
  }
}

void test_elliptic_step_matches_laplace_on_rectangle() {
  const int nx = 5;
  const int ny = 5;
  orthogrid::GridField g;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &g, &unknown);
  expect(orthogrid::solve_elliptic_steps(g, unknown.data(), 12),
         "elliptic steps ok");
  for (int j = 1; j < ny - 1; ++j) {
    for (int i = 1; i < nx - 1; ++i) {
      if (!nearly(g.x(j, i), static_cast<double>(i) * 0.25, 1e-8) ||
          !nearly(g.y(j, i), static_cast<double>(j) * 0.25, 1e-8)) {
        std::fprintf(stderr, "FAIL: elliptic (%d,%d) got (%g,%g)\n", i, j,
                     g.x(j, i), g.y(j, i));
        ++g_fails;
      }
    }
  }
}

void test_boundary_file_solves_rectangle() {
  const char* path = "orthogrid_boundary_fixture.txt";
  {
    std::ofstream out(path);
    out << "gridbnd:\n";
    out << "5 5\n";
    out << "begin\n";
    out << "main_begin\n";
    out << "2\n0 4 0 0 1 1\n0,0\n4,0\n";
    out << "2\n0 4 4 1 1 1\n4,0\n4,4\n";
    out << "2\n4 0 4 2 1 1\n4,4\n0,4\n";
    out << "2\n4 0 0 3 1 1\n0,4\n0,0\n";
    out << "main_end\nend\n";
  }
  const plugin::detail::BoundarySolve solved =
      plugin::detail::solve_grid_boundary_file(path);
  expect(solved.ok, "boundary file solves");
  expect(solved.node_count == 25, "boundary node count");
  expect(solved.message.find("\"nodes\":25") != std::string::npos,
         "boundary reports nodes");
  expect(!solved.cell_orth.empty(), "cell orth populated");
  expect(!solved.raster_orth.empty(), "raster orth populated");
  expect(solved.xs.size() == 25 && solved.ys.size() == 25,
         "boundary keeps vector coords");

  {
    std::ofstream out(path);
    out << "gridbnd:\n";
  }
  const plugin::detail::BoundarySolve header_only =
      plugin::detail::solve_grid_boundary_file(path);
  expect(!header_only.ok, "header-only file is not a laplace smoke");
  std::remove(path);
}

void test_unit_square_orthogonality_near_zero() {
  const int nx = 5;
  const int ny = 5;
  orthogrid::GridField g;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &g, &unknown);
  expect(orthogrid::solve_laplace(g, unknown.data()), "laplace for orth");
  const orthogrid::OrthogonalityField orth =
      orthogrid::compute_orthogonality(g);
  expect(orth.node_delta.size() == static_cast<size_t>(nx * ny),
         "node orth size");
  expect(orth.cell_delta.size() == static_cast<size_t>((nx - 1) * (ny - 1)),
         "cell orth size");
  float max_d = 0.f;
  for (float d : orth.node_delta) {
    if (d > max_d) {
      max_d = d;
    }
  }
  expect(max_d < 1e-3f, "rectangle interior nearly orthogonal");
  expect(std::string(orthogrid::heat_class_from_delta(0.f)) == "0",
         "heat class 0");
  expect(std::string(orthogrid::heat_class_from_delta(40.f)) == "3",
         "heat class 3");
}

}  // namespace

int main() {
  test_rejects_tiny_grid();
  test_no_unknowns_is_noop();
  test_unit_square_recovers_bilinear();
  test_elliptic_step_matches_laplace_on_rectangle();
  test_boundary_file_solves_rectangle();
  test_unit_square_orthogonality_near_zero();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/orthogrid/detail/laplace_solver.h"

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

int idx(int nx, int i, int j) {
  return j * nx + i;
}

void fill_unit_square_scrambled(int nx,
                                int ny,
                                std::vector<double>* xs,
                                std::vector<double>* ys,
                                std::vector<std::uint8_t>* unknown) {
  xs->assign(static_cast<size_t>(nx * ny), 0.0);
  ys->assign(static_cast<size_t>(nx * ny), 0.0);
  unknown->assign(static_cast<size_t>(nx * ny), 0);
  const double dx = 1.0 / static_cast<double>(nx - 1);
  const double dy = 1.0 / static_cast<double>(ny - 1);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int k = idx(nx, i, j);
      const bool interior = (i > 0 && i < nx - 1 && j > 0 && j < ny - 1);
      (*unknown)[static_cast<size_t>(k)] = interior ? 1 : 0;
      if (interior) {
        (*xs)[static_cast<size_t>(k)] = 0.37;
        (*ys)[static_cast<size_t>(k)] = 0.63;
      } else {
        (*xs)[static_cast<size_t>(k)] = static_cast<double>(i) * dx;
        (*ys)[static_cast<size_t>(k)] = static_cast<double>(j) * dy;
      }
    }
  }
}

bool nearly(double a, double b, double tol) {
  return std::fabs(a - b) <= tol;
}

void test_rejects_tiny_grid() {
  double x = 0.0;
  double y = 0.0;
  std::uint8_t unk = 1;
  orthogrid::GridField g{2, 2, &x, &y};
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
  orthogrid::GridField g{nx, ny, xs.data(), ys.data()};
  expect(orthogrid::solve_laplace(g, unknown.data()), "all-Dirichlet ok");
  expect(nearly(xs[static_cast<size_t>(idx(nx, 1, 1))], 1.0, 1e-12),
         "Dirichlet x unchanged");
  expect(nearly(ys[static_cast<size_t>(idx(nx, 1, 1))], 3.0, 1e-12),
         "Dirichlet y unchanged");
}

void test_unit_square_recovers_bilinear() {
  const int nx = 5;
  const int ny = 5;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &xs, &ys, &unknown);
  orthogrid::GridField g{nx, ny, xs.data(), ys.data()};
  expect(orthogrid::solve_laplace(g, unknown.data()), "laplace solve ok");
  const double dx = 0.25;
  const double dy = 0.25;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int k = idx(nx, i, j);
      if (!nearly(xs[static_cast<size_t>(k)], static_cast<double>(i) * dx,
                  1e-9) ||
          !nearly(ys[static_cast<size_t>(k)], static_cast<double>(j) * dy,
                  1e-9)) {
        std::fprintf(stderr, "FAIL: node (%d,%d) got (%g,%g)\n", i, j,
                     xs[static_cast<size_t>(k)], ys[static_cast<size_t>(k)]);
        ++g_fails;
      }
    }
  }
}

void test_elliptic_step_matches_laplace_on_rectangle() {
  const int nx = 5;
  const int ny = 5;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &xs, &ys, &unknown);
  orthogrid::GridField g{nx, ny, xs.data(), ys.data()};
  bool ok = false;
  for (int sweep = 0; sweep < 12; ++sweep) {
    ok = orthogrid::solve_elliptic_step(g, unknown.data());
    if (!ok) {
      break;
    }
  }
  expect(ok, "elliptic step ok");
  for (int j = 1; j < ny - 1; ++j) {
    for (int i = 1; i < nx - 1; ++i) {
      const int k = idx(nx, i, j);
      if (!nearly(xs[static_cast<size_t>(k)], static_cast<double>(i) * 0.25,
                  1e-8) ||
          !nearly(ys[static_cast<size_t>(k)], static_cast<double>(j) * 0.25,
                  1e-8)) {
        std::fprintf(stderr, "FAIL: elliptic (%d,%d) got (%g,%g)\n", i, j,
                     xs[static_cast<size_t>(k)], ys[static_cast<size_t>(k)]);
        ++g_fails;
      }
    }
  }
}

}  // namespace

int main() {
  test_rejects_tiny_grid();
  test_no_unknowns_is_noop();
  test_unit_square_recovers_bilinear();
  test_elliptic_step_matches_laplace_on_rectangle();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}

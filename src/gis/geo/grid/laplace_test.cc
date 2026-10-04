// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/grid/laplace.h"
#include "gis/geo/grid/orthogonality.h"

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

int idx2(int nx, int i, int j) {
  return j * nx + i;
}

int idx3(int nx, int ny, int i, int j, int k) {
  return k * ny * nx + j * nx + i;
}

bool nearly(double a, double b, double tol) {
  return std::fabs(a - b) <= tol;
}

void fill_unit_square_scrambled(int nx,
                                int ny,
                                std::vector<double>* xs,
                                std::vector<double>* ys,
                                std::vector<std::uint8_t>* unknown) {
  unknown->assign(static_cast<size_t>(nx * ny), 0);
  xs->assign(static_cast<size_t>(nx * ny), 0.0);
  ys->assign(static_cast<size_t>(nx * ny), 0.0);
  const double dx = 1.0 / static_cast<double>(nx - 1);
  const double dy = 1.0 / static_cast<double>(ny - 1);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int k = idx2(nx, i, j);
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

void test_rejects_tiny_grid() {
  double x[4] = {};
  double y[4] = {};
  std::uint8_t unk = 1;
  geo::NodeField2d field{2, 2, x, y};
  expect(!geo::solve_laplace(field, &unk), "tiny 2d field should fail");
}

void test_no_unknowns_is_noop() {
  const int nx = 3;
  const int ny = 3;
  std::vector<double> xs(9);
  std::vector<double> ys(9);
  std::vector<std::uint8_t> unknown(9, 0);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      xs[static_cast<size_t>(idx2(nx, i, j))] = static_cast<double>(i);
      ys[static_cast<size_t>(idx2(nx, i, j))] = static_cast<double>(j) + 2.0;
    }
  }
  geo::NodeField2d field{nx, ny, xs.data(), ys.data()};
  expect(geo::solve_laplace(field, unknown.data()), "all-Dirichlet ok");
  expect(nearly(xs[static_cast<size_t>(idx2(nx, 1, 1))], 1.0, 1e-12),
         "Dirichlet x unchanged");
  expect(nearly(ys[static_cast<size_t>(idx2(nx, 1, 1))], 3.0, 1e-12),
         "Dirichlet y unchanged");
}

void test_unit_square_recovers_bilinear() {
  const int nx = 5;
  const int ny = 5;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &xs, &ys, &unknown);
  geo::NodeField2d field{nx, ny, xs.data(), ys.data()};
  expect(geo::solve_laplace(field, unknown.data()), "laplace solve ok");
  const double dx = 0.25;
  const double dy = 0.25;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int k = idx2(nx, i, j);
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

void test_elliptic_matches_laplace_on_rectangle() {
  const int nx = 5;
  const int ny = 5;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &xs, &ys, &unknown);
  geo::NodeField2d field{nx, ny, xs.data(), ys.data()};
  expect(geo::solve_elliptic(field, unknown.data(), 12), "elliptic steps ok");
  for (int j = 1; j < ny - 1; ++j) {
    for (int i = 1; i < nx - 1; ++i) {
      const int k = idx2(nx, i, j);
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

void test_unit_box_laplace() {
  const int nx = 5;
  const int ny = 5;
  const int nz = 5;
  const int n = nx * ny * nz;
  std::vector<double> xs(static_cast<size_t>(n));
  std::vector<double> ys(static_cast<size_t>(n));
  std::vector<double> zs(static_cast<size_t>(n));
  std::vector<std::uint8_t> unknown(static_cast<size_t>(n), 0);
  const double dx = 1.0 / static_cast<double>(nx - 1);
  const double dy = 1.0 / static_cast<double>(ny - 1);
  const double dz = 1.0 / static_cast<double>(nz - 1);
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int at = idx3(nx, ny, i, j, k);
        const bool on_face = (i == 0 || i == nx - 1 || j == 0 || j == ny - 1 ||
                              k == 0 || k == nz - 1);
        unknown[static_cast<size_t>(at)] = on_face ? 0 : 1;
        if (on_face) {
          xs[static_cast<size_t>(at)] = static_cast<double>(i) * dx;
          ys[static_cast<size_t>(at)] = static_cast<double>(j) * dy;
          zs[static_cast<size_t>(at)] = static_cast<double>(k) * dz;
        } else {
          xs[static_cast<size_t>(at)] = 0.31;
          ys[static_cast<size_t>(at)] = 0.59;
          zs[static_cast<size_t>(at)] = 0.17;
        }
      }
    }
  }
  geo::NodeField3d field{nx, ny, nz, xs.data(), ys.data(), zs.data()};
  expect(geo::solve_laplace(field, unknown.data()), "3d laplace ok");
  const int mid = idx3(nx, ny, 2, 2, 2);
  expect(nearly(xs[static_cast<size_t>(mid)], 0.5, 1e-9) &&
             nearly(ys[static_cast<size_t>(mid)], 0.5, 1e-9) &&
             nearly(zs[static_cast<size_t>(mid)], 0.5, 1e-9),
         "unit box center");
}

void test_unit_square_orthogonality_near_zero() {
  const int nx = 5;
  const int ny = 5;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<std::uint8_t> unknown;
  fill_unit_square_scrambled(nx, ny, &xs, &ys, &unknown);
  geo::NodeField2d field{nx, ny, xs.data(), ys.data()};
  expect(geo::solve_laplace(field, unknown.data()), "laplace for orth");
  const geo::Orthogonality2d orth = geo::compute_orthogonality(field);
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

  std::vector<float> raster;
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  expect(geo::sample_orthogonality_raster(field, orth.node_delta.data(), 8, 8,
                                          &raster, &min_x, &min_y, &max_x,
                                          &max_y),
         "raster sample ok");
  expect(raster.size() == 64, "raster size");
  expect(nearly(min_x, 0.0, 1e-12) && nearly(max_x, 1.0, 1e-12), "raster x mbr");
}

void test_unit_box_cell_orthogonality() {
  const int nx = 5;
  const int ny = 5;
  const int nz = 5;
  const int n = nx * ny * nz;
  std::vector<double> xs(static_cast<size_t>(n));
  std::vector<double> ys(static_cast<size_t>(n));
  std::vector<double> zs(static_cast<size_t>(n));
  std::vector<std::uint8_t> unknown(static_cast<size_t>(n), 0);
  const double dx = 1.0 / static_cast<double>(nx - 1);
  const double dy = 1.0 / static_cast<double>(ny - 1);
  const double dz = 1.0 / static_cast<double>(nz - 1);
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int at = idx3(nx, ny, i, j, k);
        const bool on_face = (i == 0 || i == nx - 1 || j == 0 || j == ny - 1 ||
                              k == 0 || k == nz - 1);
        unknown[static_cast<size_t>(at)] = on_face ? 0 : 1;
        if (on_face) {
          xs[static_cast<size_t>(at)] = static_cast<double>(i) * dx;
          ys[static_cast<size_t>(at)] = static_cast<double>(j) * dy;
          zs[static_cast<size_t>(at)] = static_cast<double>(k) * dz;
        } else {
          xs[static_cast<size_t>(at)] = 0.31;
          ys[static_cast<size_t>(at)] = 0.59;
          zs[static_cast<size_t>(at)] = 0.17;
        }
      }
    }
  }
  geo::NodeField3d field{nx, ny, nz, xs.data(), ys.data(), zs.data()};
  expect(geo::solve_laplace(field, unknown.data()), "3d laplace for orth");
  const geo::Orthogonality3d orth = geo::compute_orthogonality(field);
  expect(orth.cell_delta.size() ==
             static_cast<size_t>((nx - 1) * (ny - 1) * (nz - 1)),
         "3d cell orth size");
  float max_skew = 0.f;
  for (float s : orth.cell_delta) {
    if (s > max_skew) {
      max_skew = s;
    }
  }
  expect(max_skew < 1e-3f, "unit box nearly orthogonal");
}

}  // namespace

int main() {
  test_rejects_tiny_grid();
  test_no_unknowns_is_noop();
  test_unit_square_recovers_bilinear();
  test_elliptic_matches_laplace_on_rectangle();
  test_unit_box_laplace();
  test_unit_square_orthogonality_near_zero();
  test_unit_box_cell_orthogonality();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}

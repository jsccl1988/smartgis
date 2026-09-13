// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/orthogrid/detail/laplace_solver.h"

#include <Eigen/Sparse>
#include <Eigen/SparseLU>

#include <vector>

namespace orthogrid {
namespace detail {

constexpr double kDegenerate = 1e-18;

int node_index(int nx, int i, int j) {
  return j * nx + i;
}

bool grid_is_usable(const GridField& grid, const std::uint8_t* is_unknown) {
  return grid.nx >= 3 && grid.ny >= 3 && grid.x != nullptr &&
         grid.y != nullptr && is_unknown != nullptr;
}

void map_unknowns(int nx,
                  int ny,
                  const std::uint8_t* is_unknown,
                  std::vector<int>* unk_id,
                  int* nunk) {
  unk_id->assign(static_cast<size_t>(nx * ny), -1);
  *nunk = 0;
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int k = node_index(nx, i, j);
      if (is_unknown[k] != 0) {
        (*unk_id)[static_cast<size_t>(k)] = (*nunk)++;
      }
    }
  }
}

void add_entry(int row,
               int ni,
               int nj,
               int nx,
               int ny,
               double coeff,
               const std::vector<int>& unk_id,
               const double* xs,
               const double* ys,
               std::vector<Eigen::Triplet<double>>* trips,
               Eigen::VectorXd* bx,
               Eigen::VectorXd* by) {
  if (coeff == 0.0 || ni < 0 || nj < 0 || ni >= nx || nj >= ny) {
    return;
  }
  const int k = node_index(nx, ni, nj);
  const int col = unk_id[static_cast<size_t>(k)];
  if (col >= 0) {
    trips->emplace_back(row, col, coeff);
    return;
  }
  (*bx)(row) -= coeff * xs[k];
  (*by)(row) -= coeff * ys[k];
}

bool factor_and_write(const std::vector<Eigen::Triplet<double>>& trips,
                      const Eigen::VectorXd& bx,
                      const Eigen::VectorXd& by,
                      const std::vector<int>& unk_id,
                      int nunk,
                      const GridField& grid) {
  Eigen::SparseMatrix<double> a(nunk, nunk);
  a.setFromTriplets(trips.begin(), trips.end());
  a.makeCompressed();

  Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
  solver.analyzePattern(a);
  solver.factorize(a);
  if (solver.info() != Eigen::Success) {
    return false;
  }

  const Eigen::VectorXd ux = solver.solve(bx);
  const Eigen::VectorXd uy = solver.solve(by);
  if (solver.info() != Eigen::Success) {
    return false;
  }

  const int n = grid.nx * grid.ny;
  for (int k = 0; k < n; ++k) {
    const int row = unk_id[static_cast<size_t>(k)];
    if (row < 0) {
      continue;
    }
    grid.x[k] = ux(row);
    grid.y[k] = uy(row);
  }
  return true;
}

bool solve_five_point(const GridField& grid, const std::uint8_t* is_unknown) {
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(grid.nx, grid.ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }

  std::vector<Eigen::Triplet<double>> trips;
  trips.reserve(static_cast<size_t>(nunk) * 5);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);

  for (int j = 0; j < grid.ny; ++j) {
    for (int i = 0; i < grid.nx; ++i) {
      const int k = node_index(grid.nx, i, j);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      add_entry(row, i, j, grid.nx, grid.ny, 4.0, unk_id, grid.x, grid.y, &trips,
                &bx, &by);
      add_entry(row, i + 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i - 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i, j + 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i, j - 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
    }
  }
  return factor_and_write(trips, bx, by, unk_id, nunk, grid);
}

void metrics_at(const GridField& grid,
                int i,
                int j,
                double* alpha,
                double* beta,
                double* gamma) {
  const int e = node_index(grid.nx, i + 1, j);
  const int w = node_index(grid.nx, i - 1, j);
  const int n = node_index(grid.nx, i, j + 1);
  const int s = node_index(grid.nx, i, j - 1);
  const double xks = (grid.x[e] - grid.x[w]) * 0.5;
  const double yks = (grid.y[e] - grid.y[w]) * 0.5;
  const double xat = (grid.x[n] - grid.x[s]) * 0.5;
  const double yat = (grid.y[n] - grid.y[s]) * 0.5;
  *alpha = xat * xat + yat * yat;
  *gamma = xks * xks + yks * yks;
  *beta = xks * xat + yks * yat;
}

bool solve_thompson(const GridField& grid, const std::uint8_t* is_unknown) {
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(grid.nx, grid.ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }

  std::vector<Eigen::Triplet<double>> trips;
  trips.reserve(static_cast<size_t>(nunk) * 9);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);

  for (int j = 0; j < grid.ny; ++j) {
    for (int i = 0; i < grid.nx; ++i) {
      const int k = node_index(grid.nx, i, j);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      if (i < 1 || i >= grid.nx - 1 || j < 1 || j >= grid.ny - 1) {
        add_entry(row, i, j, grid.nx, grid.ny, 1.0, unk_id, grid.x, grid.y,
                  &trips, &bx, &by);
        continue;
      }

      double alpha = 0.0;
      double beta = 0.0;
      double gamma = 0.0;
      metrics_at(grid, i, j, &alpha, &beta, &gamma);
      if (alpha + gamma < kDegenerate) {
        add_entry(row, i, j, grid.nx, grid.ny, 4.0, unk_id, grid.x, grid.y,
                  &trips, &bx, &by);
        add_entry(row, i + 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  &trips, &bx, &by);
        add_entry(row, i - 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  &trips, &bx, &by);
        add_entry(row, i, j + 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  &trips, &bx, &by);
        add_entry(row, i, j - 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  &trips, &bx, &by);
        continue;
      }

      const double half_beta = 0.5 * beta;
      add_entry(row, i, j, grid.nx, grid.ny, -2.0 * (alpha + gamma), unk_id,
                grid.x, grid.y, &trips, &bx, &by);
      add_entry(row, i + 1, j, grid.nx, grid.ny, alpha, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i - 1, j, grid.nx, grid.ny, alpha, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i, j + 1, grid.nx, grid.ny, gamma, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i, j - 1, grid.nx, grid.ny, gamma, unk_id, grid.x, grid.y,
                &trips, &bx, &by);
      add_entry(row, i + 1, j + 1, grid.nx, grid.ny, -half_beta, unk_id, grid.x,
                grid.y, &trips, &bx, &by);
      add_entry(row, i - 1, j - 1, grid.nx, grid.ny, -half_beta, unk_id, grid.x,
                grid.y, &trips, &bx, &by);
      add_entry(row, i + 1, j - 1, grid.nx, grid.ny, half_beta, unk_id, grid.x,
                grid.y, &trips, &bx, &by);
      add_entry(row, i - 1, j + 1, grid.nx, grid.ny, half_beta, unk_id, grid.x,
                grid.y, &trips, &bx, &by);
    }
  }
  return factor_and_write(trips, bx, by, unk_id, nunk, grid);
}

}  // namespace detail

bool solve_laplace(const GridField& grid, const std::uint8_t* is_unknown) {
  if (!detail::grid_is_usable(grid, is_unknown)) {
    return false;
  }
  return detail::solve_five_point(grid, is_unknown);
}

bool solve_elliptic_step(const GridField& grid,
                         const std::uint8_t* is_unknown) {
  if (!detail::grid_is_usable(grid, is_unknown)) {
    return false;
  }
  return detail::solve_thompson(grid, is_unknown);
}

}  // namespace orthogrid

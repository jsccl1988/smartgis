// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid/detail/laplace_solver.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <Eigen/SparseLU>

#include <vector>

namespace orthogrid {
namespace detail {

constexpr double kDegenerate = 1e-18;

using Triplet = Eigen::Triplet<double>;
using SpMat = Eigen::SparseMatrix<double>;

int node_index(int nx, int i, int j) {
  return j * nx + i;
}

bool grid_is_usable(const GridField& grid, const std::uint8_t* is_unknown) {
  return grid.is_shaped() && is_unknown != nullptr;
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
               const GridArray& xs,
               const GridArray& ys,
               std::vector<Triplet>* trips,
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
  (*bx)(row) -= coeff * xs(nj, ni);
  (*by)(row) -= coeff * ys(nj, ni);
}

void write_solution(const Eigen::VectorXd& ux,
                    const Eigen::VectorXd& uy,
                    const std::vector<int>& unk_id,
                    GridField* grid) {
  const int n = grid->nx * grid->ny;
  for (int k = 0; k < n; ++k) {
    const int row = unk_id[static_cast<size_t>(k)];
    if (row < 0) {
      continue;
    }
    const int i = k % grid->nx;
    const int j = k / grid->nx;
    grid->x(j, i) = ux(row);
    grid->y(j, i) = uy(row);
  }
}

template <typename Solver>
bool factor_solve_write(Solver* solver,
                        SpMat* a,
                        const std::vector<Triplet>& trips,
                        const Eigen::VectorXd& bx,
                        const Eigen::VectorXd& by,
                        const std::vector<int>& unk_id,
                        int nunk,
                        bool analyze,
                        GridField* grid) {
  a->resize(nunk, nunk);
  a->setFromTriplets(trips.begin(), trips.end());
  a->makeCompressed();
  if (analyze) {
    solver->analyzePattern(*a);
  }
  solver->factorize(*a);
  if (solver->info() != Eigen::Success) {
    return false;
  }
  const Eigen::VectorXd ux = solver->solve(bx);
  const Eigen::VectorXd uy = solver->solve(by);
  if (solver->info() != Eigen::Success) {
    return false;
  }
  write_solution(ux, uy, unk_id, grid);
  return true;
}

void assemble_five_point(const GridField& grid,
                         const std::vector<int>& unk_id,
                         std::vector<Triplet>* trips,
                         Eigen::VectorXd* bx,
                         Eigen::VectorXd* by) {
  for (int j = 0; j < grid.ny; ++j) {
    for (int i = 0; i < grid.nx; ++i) {
      const int k = node_index(grid.nx, i, j);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      add_entry(row, i, j, grid.nx, grid.ny, 4.0, unk_id, grid.x, grid.y, trips,
                bx, by);
      add_entry(row, i + 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i - 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i, j + 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i, j - 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                trips, bx, by);
    }
  }
}

void metrics_at(const GridField& grid,
                int i,
                int j,
                double* alpha,
                double* beta,
                double* gamma) {
  const Eigen::Vector2d ksi(grid.x(j, i + 1) - grid.x(j, i - 1),
                            grid.y(j, i + 1) - grid.y(j, i - 1));
  const Eigen::Vector2d eta(grid.x(j + 1, i) - grid.x(j - 1, i),
                            grid.y(j + 1, i) - grid.y(j - 1, i));
  const Eigen::Vector2d half_ksi = 0.5 * ksi;
  const Eigen::Vector2d half_eta = 0.5 * eta;
  *alpha = half_eta.squaredNorm();
  *gamma = half_ksi.squaredNorm();
  *beta = half_ksi.dot(half_eta);
}

void assemble_thompson(const GridField& grid,
                       const std::vector<int>& unk_id,
                       std::vector<Triplet>* trips,
                       Eigen::VectorXd* bx,
                       Eigen::VectorXd* by) {
  for (int j = 0; j < grid.ny; ++j) {
    for (int i = 0; i < grid.nx; ++i) {
      const int k = node_index(grid.nx, i, j);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      if (i < 1 || i >= grid.nx - 1 || j < 1 || j >= grid.ny - 1) {
        add_entry(row, i, j, grid.nx, grid.ny, 1.0, unk_id, grid.x, grid.y,
                  trips, bx, by);
        continue;
      }

      double alpha = 0.0;
      double beta = 0.0;
      double gamma = 0.0;
      metrics_at(grid, i, j, &alpha, &beta, &gamma);
      if (alpha + gamma < kDegenerate) {
        add_entry(row, i, j, grid.nx, grid.ny, 4.0, unk_id, grid.x, grid.y,
                  trips, bx, by);
        add_entry(row, i + 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  trips, bx, by);
        add_entry(row, i - 1, j, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  trips, bx, by);
        add_entry(row, i, j + 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  trips, bx, by);
        add_entry(row, i, j - 1, grid.nx, grid.ny, -1.0, unk_id, grid.x, grid.y,
                  trips, bx, by);
        continue;
      }

      const double half_beta = 0.5 * beta;
      add_entry(row, i, j, grid.nx, grid.ny, -2.0 * (alpha + gamma), unk_id,
                grid.x, grid.y, trips, bx, by);
      add_entry(row, i + 1, j, grid.nx, grid.ny, alpha, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i - 1, j, grid.nx, grid.ny, alpha, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i, j + 1, grid.nx, grid.ny, gamma, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i, j - 1, grid.nx, grid.ny, gamma, unk_id, grid.x, grid.y,
                trips, bx, by);
      add_entry(row, i + 1, j + 1, grid.nx, grid.ny, -half_beta, unk_id, grid.x,
                grid.y, trips, bx, by);
      add_entry(row, i - 1, j - 1, grid.nx, grid.ny, -half_beta, unk_id, grid.x,
                grid.y, trips, bx, by);
      add_entry(row, i + 1, j - 1, grid.nx, grid.ny, half_beta, unk_id, grid.x,
                grid.y, trips, bx, by);
      add_entry(row, i - 1, j + 1, grid.nx, grid.ny, half_beta, unk_id, grid.x,
                grid.y, trips, bx, by);
    }
  }
}

bool solve_five_point(GridField* grid, const std::uint8_t* is_unknown) {
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(grid->nx, grid->ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }

  std::vector<Triplet> trips;
  trips.reserve(static_cast<size_t>(nunk) * 5);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);
  assemble_five_point(*grid, unk_id, &trips, &bx, &by);

  SpMat a;
  Eigen::SimplicialLDLT<SpMat> solver;
  return factor_solve_write(&solver, &a, trips, bx, by, unk_id, nunk,
                            /*analyze=*/true, grid);
}

bool solve_thompson_once(GridField* grid,
                         const std::vector<int>& unk_id,
                         int nunk,
                         Eigen::SparseLU<SpMat>* solver,
                         SpMat* a,
                         bool analyze) {
  std::vector<Triplet> trips;
  trips.reserve(static_cast<size_t>(nunk) * 9);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);
  assemble_thompson(*grid, unk_id, &trips, &bx, &by);
  return factor_solve_write(solver, a, trips, bx, by, unk_id, nunk, analyze,
                            grid);
}

}  // namespace detail

void GridField::assign_from_flat(int nx_in,
                                 int ny_in,
                                 const double* xs,
                                 const double* ys) {
  nx = nx_in;
  ny = ny_in;
  x.resize(ny, nx);
  y.resize(ny, nx);
  if (!xs || !ys || nx <= 0 || ny <= 0) {
    return;
  }
  Eigen::Map<const GridArray> mx(xs, ny, nx);
  Eigen::Map<const GridArray> my(ys, ny, nx);
  x = mx;
  y = my;
}

void GridField::copy_to_flat(std::vector<double>* xs,
                            std::vector<double>* ys) const {
  if (!xs || !ys) {
    return;
  }
  const size_t n = static_cast<size_t>(nx) * static_cast<size_t>(ny);
  xs->resize(n);
  ys->resize(n);
  if (n == 0 || !is_shaped()) {
    return;
  }
  Eigen::Map<GridArray> mx(xs->data(), ny, nx);
  Eigen::Map<GridArray> my(ys->data(), ny, nx);
  mx = x;
  my = y;
}

bool solve_laplace(GridField& grid, const std::uint8_t* is_unknown) {
  if (!detail::grid_is_usable(grid, is_unknown)) {
    return false;
  }
  return detail::solve_five_point(&grid, is_unknown);
}

bool solve_elliptic_step(GridField& grid, const std::uint8_t* is_unknown) {
  if (!detail::grid_is_usable(grid, is_unknown)) {
    return false;
  }
  std::vector<int> unk_id;
  int nunk = 0;
  detail::map_unknowns(grid.nx, grid.ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }
  detail::SpMat a;
  Eigen::SparseLU<detail::SpMat> solver;
  return detail::solve_thompson_once(&grid, unk_id, nunk, &solver, &a,
                                     /*analyze=*/true);
}

bool solve_elliptic_steps(GridField& grid,
                          const std::uint8_t* is_unknown,
                          int iters) {
  if (iters <= 0) {
    return true;
  }
  if (!detail::grid_is_usable(grid, is_unknown)) {
    return false;
  }
  std::vector<int> unk_id;
  int nunk = 0;
  detail::map_unknowns(grid.nx, grid.ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }
  detail::SpMat a;
  Eigen::SparseLU<detail::SpMat> solver;
  for (int s = 0; s < iters; ++s) {
    if (!detail::solve_thompson_once(&grid, unk_id, nunk, &solver, &a,
                                     /*analyze=*/s == 0)) {
      return false;
    }
  }
  return true;
}

}  // namespace orthogrid

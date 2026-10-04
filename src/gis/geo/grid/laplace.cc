// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/grid/laplace.h"

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <Eigen/SparseLU>

#include <vector>

namespace geo {
namespace detail {
namespace {

constexpr double kDegenerate = 1e-18;

using Triplet = Eigen::Triplet<double>;
using SpMat = Eigen::SparseMatrix<double>;

bool field2d_usable(const NodeField2d& field, const std::uint8_t* is_unknown) {
  return field.nx >= 3 && field.ny >= 3 && field.x != nullptr &&
         field.y != nullptr && is_unknown != nullptr;
}

bool field3d_usable(const NodeField3d& field, const std::uint8_t* is_unknown) {
  return field.nx >= 3 && field.ny >= 3 && field.nz >= 3 && field.x != nullptr &&
         field.y != nullptr && field.z != nullptr && is_unknown != nullptr;
}

void map_unknowns(int n, const std::uint8_t* is_unknown, std::vector<int>* unk_id,
                  int* nunk) {
  unk_id->assign(static_cast<size_t>(n), -1);
  *nunk = 0;
  for (int idx = 0; idx < n; ++idx) {
    if (is_unknown[idx] != 0) {
      (*unk_id)[static_cast<size_t>(idx)] = (*nunk)++;
    }
  }
}

void add_entry_2d(int row,
                  int ni,
                  int nj,
                  const NodeField2d& field,
                  double coeff,
                  const std::vector<int>& unk_id,
                  std::vector<Triplet>* trips,
                  Eigen::VectorXd* bx,
                  Eigen::VectorXd* by) {
  if (coeff == 0.0 || ni < 0 || nj < 0 || ni >= field.nx || nj >= field.ny) {
    return;
  }
  const int k = node_index_2d(field.nx, ni, nj);
  const int col = unk_id[static_cast<size_t>(k)];
  if (col >= 0) {
    trips->emplace_back(row, col, coeff);
    return;
  }
  (*bx)(row) -= coeff * field.x[k];
  (*by)(row) -= coeff * field.y[k];
}

void write_solution_2d(const Eigen::VectorXd& ux,
                       const Eigen::VectorXd& uy,
                       const std::vector<int>& unk_id,
                       NodeField2d* field) {
  const int n = field->nx * field->ny;
  for (int k = 0; k < n; ++k) {
    const int row = unk_id[static_cast<size_t>(k)];
    if (row < 0) {
      continue;
    }
    field->x[k] = ux(row);
    field->y[k] = uy(row);
  }
}

template <typename Solver>
bool factor_solve_write_2d(Solver* solver,
                           SpMat* a,
                           const std::vector<Triplet>& trips,
                           const Eigen::VectorXd& bx,
                           const Eigen::VectorXd& by,
                           const std::vector<int>& unk_id,
                           int nunk,
                           bool analyze,
                           NodeField2d* field) {
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
  write_solution_2d(ux, uy, unk_id, field);
  return true;
}

void assemble_five_point(const NodeField2d& field,
                         const std::vector<int>& unk_id,
                         std::vector<Triplet>* trips,
                         Eigen::VectorXd* bx,
                         Eigen::VectorXd* by) {
  for (int j = 0; j < field.ny; ++j) {
    for (int i = 0; i < field.nx; ++i) {
      const int k = node_index_2d(field.nx, i, j);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      add_entry_2d(row, i, j, field, 4.0, unk_id, trips, bx, by);
      add_entry_2d(row, i + 1, j, field, -1.0, unk_id, trips, bx, by);
      add_entry_2d(row, i - 1, j, field, -1.0, unk_id, trips, bx, by);
      add_entry_2d(row, i, j + 1, field, -1.0, unk_id, trips, bx, by);
      add_entry_2d(row, i, j - 1, field, -1.0, unk_id, trips, bx, by);
    }
  }
}

void metrics_at(const NodeField2d& field,
                int i,
                int j,
                double* alpha,
                double* beta,
                double* gamma) {
  const int ip = node_index_2d(field.nx, i + 1, j);
  const int im = node_index_2d(field.nx, i - 1, j);
  const int jp = node_index_2d(field.nx, i, j + 1);
  const int jm = node_index_2d(field.nx, i, j - 1);
  const Eigen::Vector2d ksi(field.x[ip] - field.x[im], field.y[ip] - field.y[im]);
  const Eigen::Vector2d eta(field.x[jp] - field.x[jm], field.y[jp] - field.y[jm]);
  const Eigen::Vector2d half_ksi = 0.5 * ksi;
  const Eigen::Vector2d half_eta = 0.5 * eta;
  *alpha = half_eta.squaredNorm();
  *gamma = half_ksi.squaredNorm();
  *beta = half_ksi.dot(half_eta);
}

void assemble_thompson(const NodeField2d& field,
                       const std::vector<int>& unk_id,
                       std::vector<Triplet>* trips,
                       Eigen::VectorXd* bx,
                       Eigen::VectorXd* by) {
  for (int j = 0; j < field.ny; ++j) {
    for (int i = 0; i < field.nx; ++i) {
      const int k = node_index_2d(field.nx, i, j);
      const int row = unk_id[static_cast<size_t>(k)];
      if (row < 0) {
        continue;
      }
      if (i < 1 || i >= field.nx - 1 || j < 1 || j >= field.ny - 1) {
        add_entry_2d(row, i, j, field, 1.0, unk_id, trips, bx, by);
        continue;
      }

      double alpha = 0.0;
      double beta = 0.0;
      double gamma = 0.0;
      metrics_at(field, i, j, &alpha, &beta, &gamma);
      if (alpha + gamma < kDegenerate) {
        add_entry_2d(row, i, j, field, 4.0, unk_id, trips, bx, by);
        add_entry_2d(row, i + 1, j, field, -1.0, unk_id, trips, bx, by);
        add_entry_2d(row, i - 1, j, field, -1.0, unk_id, trips, bx, by);
        add_entry_2d(row, i, j + 1, field, -1.0, unk_id, trips, bx, by);
        add_entry_2d(row, i, j - 1, field, -1.0, unk_id, trips, bx, by);
        continue;
      }

      const double half_beta = 0.5 * beta;
      add_entry_2d(row, i, j, field, -2.0 * (alpha + gamma), unk_id, trips, bx,
                   by);
      add_entry_2d(row, i + 1, j, field, alpha, unk_id, trips, bx, by);
      add_entry_2d(row, i - 1, j, field, alpha, unk_id, trips, bx, by);
      add_entry_2d(row, i, j + 1, field, gamma, unk_id, trips, bx, by);
      add_entry_2d(row, i, j - 1, field, gamma, unk_id, trips, bx, by);
      add_entry_2d(row, i + 1, j + 1, field, -half_beta, unk_id, trips, bx, by);
      add_entry_2d(row, i - 1, j - 1, field, -half_beta, unk_id, trips, bx, by);
      add_entry_2d(row, i + 1, j - 1, field, half_beta, unk_id, trips, bx, by);
      add_entry_2d(row, i - 1, j + 1, field, half_beta, unk_id, trips, bx, by);
    }
  }
}

bool solve_five_point(NodeField2d* field, const std::uint8_t* is_unknown) {
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(field->nx * field->ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }

  std::vector<Triplet> trips;
  trips.reserve(static_cast<size_t>(nunk) * 5);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);
  assemble_five_point(*field, unk_id, &trips, &bx, &by);

  SpMat a;
  Eigen::SimplicialLDLT<SpMat> solver;
  return factor_solve_write_2d(&solver, &a, trips, bx, by, unk_id, nunk,
                               /*analyze=*/true, field);
}

bool solve_thompson_once(NodeField2d* field,
                         const std::vector<int>& unk_id,
                         int nunk,
                         Eigen::SparseLU<SpMat>* solver,
                         SpMat* a,
                         bool analyze) {
  std::vector<Triplet> trips;
  trips.reserve(static_cast<size_t>(nunk) * 9);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);
  assemble_thompson(*field, unk_id, &trips, &bx, &by);
  return factor_solve_write_2d(solver, a, trips, bx, by, unk_id, nunk, analyze,
                               field);
}

bool solve_elliptic_entry(NodeField2d* field,
                          const std::uint8_t* is_unknown,
                          int iters) {
  if (iters <= 0) {
    return true;
  }
  if (!field2d_usable(*field, is_unknown)) {
    return false;
  }
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(field->nx * field->ny, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }
  SpMat a;
  Eigen::SparseLU<SpMat> solver;
  for (int s = 0; s < iters; ++s) {
    if (!solve_thompson_once(field, unk_id, nunk, &solver, &a,
                             /*analyze=*/s == 0)) {
      return false;
    }
  }
  return true;
}

void add_entry_3d(int row,
                  int ni,
                  int nj,
                  int nk,
                  const NodeField3d& field,
                  double coeff,
                  const std::vector<int>& unk_id,
                  std::vector<Triplet>* trips,
                  Eigen::VectorXd* bx,
                  Eigen::VectorXd* by,
                  Eigen::VectorXd* bz) {
  if (coeff == 0.0 || ni < 0 || nj < 0 || nk < 0 || ni >= field.nx ||
      nj >= field.ny || nk >= field.nz) {
    return;
  }
  const int idx = node_index_3d(field.nx, field.ny, ni, nj, nk);
  const int col = unk_id[static_cast<size_t>(idx)];
  if (col >= 0) {
    trips->emplace_back(row, col, coeff);
    return;
  }
  (*bx)(row) -= coeff * field.x[idx];
  (*by)(row) -= coeff * field.y[idx];
  (*bz)(row) -= coeff * field.z[idx];
}

bool solve_seven_point(const NodeField3d& field, const std::uint8_t* is_unknown) {
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(field.nx * field.ny * field.nz, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }

  std::vector<Triplet> trips;
  trips.reserve(static_cast<size_t>(nunk) * 7);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd bz = Eigen::VectorXd::Zero(nunk);

  for (int k = 0; k < field.nz; ++k) {
    for (int j = 0; j < field.ny; ++j) {
      for (int i = 0; i < field.nx; ++i) {
        const int idx = node_index_3d(field.nx, field.ny, i, j, k);
        const int row = unk_id[static_cast<size_t>(idx)];
        if (row < 0) {
          continue;
        }
        trips.emplace_back(row, row, 6.0);
        add_entry_3d(row, i - 1, j, k, field, -1.0, unk_id, &trips, &bx, &by,
                     &bz);
        add_entry_3d(row, i + 1, j, k, field, -1.0, unk_id, &trips, &bx, &by,
                     &bz);
        add_entry_3d(row, i, j - 1, k, field, -1.0, unk_id, &trips, &bx, &by,
                     &bz);
        add_entry_3d(row, i, j + 1, k, field, -1.0, unk_id, &trips, &bx, &by,
                     &bz);
        add_entry_3d(row, i, j, k - 1, field, -1.0, unk_id, &trips, &bx, &by,
                     &bz);
        add_entry_3d(row, i, j, k + 1, field, -1.0, unk_id, &trips, &bx, &by,
                     &bz);
      }
    }
  }

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
  const Eigen::VectorXd uz = solver.solve(bz);
  if (solver.info() != Eigen::Success) {
    return false;
  }

  const int n = field.nx * field.ny * field.nz;
  for (int idx = 0; idx < n; ++idx) {
    const int row = unk_id[static_cast<size_t>(idx)];
    if (row < 0) {
      continue;
    }
    field.x[idx] = ux(row);
    field.y[idx] = uy(row);
    field.z[idx] = uz(row);
  }
  return true;
}

}  // namespace
}  // namespace detail

bool solve_laplace(NodeField2d field, const std::uint8_t* is_unknown) {
  if (!detail::field2d_usable(field, is_unknown)) {
    return false;
  }
  return detail::solve_five_point(&field, is_unknown);
}

bool solve_laplace(NodeField3d field, const std::uint8_t* is_unknown) {
  if (!detail::field3d_usable(field, is_unknown)) {
    return false;
  }
  return detail::solve_seven_point(field, is_unknown);
}

bool solve_elliptic(NodeField2d field,
                    const std::uint8_t* is_unknown,
                    int iters) {
  return detail::solve_elliptic_entry(&field, is_unknown, iters);
}

}  // namespace geo

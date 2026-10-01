// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid3d/detail/laplace_solver.h"

#include <Eigen/Sparse>
#include <Eigen/SparseLU>

#include <vector>

namespace orthogrid3d {
namespace detail {
namespace {

int node_index(int nx, int ny, int i, int j, int k) {
  return k * ny * nx + j * nx + i;
}

bool field_is_usable(const VolumeField& field, const std::uint8_t* is_unknown) {
  return field.nx >= 3 && field.ny >= 3 && field.nz >= 3 && field.x != nullptr &&
         field.y != nullptr && field.z != nullptr && is_unknown != nullptr;
}

void map_unknowns(int nx,
                  int ny,
                  int nz,
                  const std::uint8_t* is_unknown,
                  std::vector<int>* unk_id,
                  int* nunk) {
  const int n = nx * ny * nz;
  unk_id->assign(static_cast<size_t>(n), -1);
  *nunk = 0;
  for (int idx = 0; idx < n; ++idx) {
    if (is_unknown[idx] != 0) {
      (*unk_id)[static_cast<size_t>(idx)] = (*nunk)++;
    }
  }
}

void add_entry(int row,
               int ni,
               int nj,
               int nk,
               int nx,
               int ny,
               int nz,
               double coeff,
               const std::vector<int>& unk_id,
               const double* xs,
               const double* ys,
               const double* zs,
               std::vector<Eigen::Triplet<double>>* trips,
               Eigen::VectorXd* bx,
               Eigen::VectorXd* by,
               Eigen::VectorXd* bz) {
  if (coeff == 0.0 || ni < 0 || nj < 0 || nk < 0 || ni >= nx || nj >= ny ||
      nk >= nz) {
    return;
  }
  const int idx = node_index(nx, ny, ni, nj, nk);
  const int col = unk_id[static_cast<size_t>(idx)];
  if (col >= 0) {
    trips->emplace_back(row, col, coeff);
    return;
  }
  (*bx)(row) -= coeff * xs[idx];
  (*by)(row) -= coeff * ys[idx];
  (*bz)(row) -= coeff * zs[idx];
}

bool factor_and_write(const std::vector<Eigen::Triplet<double>>& trips,
                      const Eigen::VectorXd& bx,
                      const Eigen::VectorXd& by,
                      const Eigen::VectorXd& bz,
                      const std::vector<int>& unk_id,
                      int nunk,
                      const VolumeField& field) {
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

bool solve_seven_point(const VolumeField& field,
                       const std::uint8_t* is_unknown) {
  std::vector<int> unk_id;
  int nunk = 0;
  map_unknowns(field.nx, field.ny, field.nz, is_unknown, &unk_id, &nunk);
  if (nunk == 0) {
    return true;
  }

  std::vector<Eigen::Triplet<double>> trips;
  trips.reserve(static_cast<size_t>(nunk) * 7);
  Eigen::VectorXd bx = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd by = Eigen::VectorXd::Zero(nunk);
  Eigen::VectorXd bz = Eigen::VectorXd::Zero(nunk);

  for (int k = 0; k < field.nz; ++k) {
    for (int j = 0; j < field.ny; ++j) {
      for (int i = 0; i < field.nx; ++i) {
        const int idx = node_index(field.nx, field.ny, i, j, k);
        const int row = unk_id[static_cast<size_t>(idx)];
        if (row < 0) {
          continue;
        }
        trips.emplace_back(row, row, 6.0);
        add_entry(row, i - 1, j, k, field.nx, field.ny, field.nz, -1.0, unk_id,
                  field.x, field.y, field.z, &trips, &bx, &by, &bz);
        add_entry(row, i + 1, j, k, field.nx, field.ny, field.nz, -1.0, unk_id,
                  field.x, field.y, field.z, &trips, &bx, &by, &bz);
        add_entry(row, i, j - 1, k, field.nx, field.ny, field.nz, -1.0, unk_id,
                  field.x, field.y, field.z, &trips, &bx, &by, &bz);
        add_entry(row, i, j + 1, k, field.nx, field.ny, field.nz, -1.0, unk_id,
                  field.x, field.y, field.z, &trips, &bx, &by, &bz);
        add_entry(row, i, j, k - 1, field.nx, field.ny, field.nz, -1.0, unk_id,
                  field.x, field.y, field.z, &trips, &bx, &by, &bz);
        add_entry(row, i, j, k + 1, field.nx, field.ny, field.nz, -1.0, unk_id,
                  field.x, field.y, field.z, &trips, &bx, &by, &bz);
      }
    }
  }

  return factor_and_write(trips, bx, by, bz, unk_id, nunk, field);
}

}  // namespace

bool solve_laplace_entry(const VolumeField& field,
                         const std::uint8_t* is_unknown) {
  if (!field_is_usable(field, is_unknown)) {
    return false;
  }
  return solve_seven_point(field, is_unknown);
}

}  // namespace detail

bool solve_laplace_3d(const VolumeField& field,
                      const std::uint8_t* is_unknown) {
  return detail::solve_laplace_entry(field, is_unknown);
}

}  // namespace orthogrid3d

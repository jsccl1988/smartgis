// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/grid/orthogonality.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace geo {
namespace detail {
namespace {

constexpr double kEps = 1e-18;
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;

bool field2d_shaped(const NodeField2d& field) {
  return field.nx >= 3 && field.ny >= 3 && field.x != nullptr &&
         field.y != nullptr;
}

float angle_skew_deg(double ax, double ay, double az, double bx, double by,
                     double bz) {
  const double la = std::sqrt(ax * ax + ay * ay + az * az);
  const double lb = std::sqrt(bx * bx + by * by + bz * bz);
  if (la < kEps || lb < kEps) {
    return 90.0f;
  }
  double c = (ax * bx + ay * by + az * bz) / (la * lb);
  c = std::max(-1.0, std::min(1.0, c));
  const double theta_deg = std::acos(c) * kRadToDeg;
  return static_cast<float>(std::fabs(90.0 - theta_deg));
}

}  // namespace
}  // namespace detail

Orthogonality2d compute_orthogonality(NodeField2d field) {
  Orthogonality2d out;
  if (!detail::field2d_shaped(field)) {
    return out;
  }
  const size_t n = static_cast<size_t>(field.nx) * static_cast<size_t>(field.ny);
  out.node_delta.assign(n, 0.f);
  for (int j = 1; j < field.ny - 1; ++j) {
    for (int i = 1; i < field.nx - 1; ++i) {
      const int ip = detail::node_index_2d(field.nx, i + 1, j);
      const int im = detail::node_index_2d(field.nx, i - 1, j);
      const int jp = detail::node_index_2d(field.nx, i, j + 1);
      const int jm = detail::node_index_2d(field.nx, i, j - 1);
      const double ksi_x = 0.5 * (field.x[ip] - field.x[im]);
      const double ksi_y = 0.5 * (field.y[ip] - field.y[im]);
      const double eta_x = 0.5 * (field.x[jp] - field.x[jm]);
      const double eta_y = 0.5 * (field.y[jp] - field.y[jm]);
      const double arfa = eta_x * eta_x + eta_y * eta_y;
      const double gama = ksi_x * ksi_x + ksi_y * ksi_y;
      const double beta = ksi_x * eta_x + ksi_y * eta_y;
      const double denom = std::sqrt(arfa * gama);
      float delta = 90.f;
      if (denom > detail::kEps) {
        double c = beta / denom;
        c = std::max(-1.0, std::min(1.0, c));
        const float theta =
            static_cast<float>(std::acos(c) * 180.0 / detail::kPi);
        delta = std::fabs(90.f - theta);
      }
      out.node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i, j))] = delta;
    }
  }

  const int cx = field.nx - 1;
  const int cy = field.ny - 1;
  out.cell_delta.assign(static_cast<size_t>(cx * cy), 0.f);
  for (int j = 0; j < cy; ++j) {
    for (int i = 0; i < cx; ++i) {
      const float a = out.node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i, j))];
      const float b = out.node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i + 1, j))];
      const float c = out.node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i + 1, j + 1))];
      const float d = out.node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i, j + 1))];
      out.cell_delta[static_cast<size_t>(j * cx + i)] = (a + b + c + d) * 0.25f;
    }
  }
  return out;
}

Orthogonality3d compute_orthogonality(NodeField3d field) {
  Orthogonality3d out;
  if (field.nx < 2 || field.ny < 2 || field.nz < 2 || field.x == nullptr ||
      field.y == nullptr || field.z == nullptr) {
    return out;
  }
  const int cx = field.nx - 1;
  const int cy = field.ny - 1;
  const int cz = field.nz - 1;
  out.cell_delta.assign(static_cast<size_t>(cx * cy * cz), 0.0f);

  auto at = [&](int i, int j, int k) {
    const int idx = detail::node_index_3d(field.nx, field.ny, i, j, k);
    return idx;
  };

  for (int k = 0; k < cz; ++k) {
    for (int j = 0; j < cy; ++j) {
      for (int i = 0; i < cx; ++i) {
        const int p000 = at(i, j, k);
        const int pi = at(i + 1, j, k);
        const int pj = at(i, j + 1, k);
        const int pk = at(i, j, k + 1);
        const double x0 = field.x[p000];
        const double y0 = field.y[p000];
        const double z0 = field.z[p000];
        const float s =
            (detail::angle_skew_deg(field.x[pi] - x0, field.y[pi] - y0,
                                    field.z[pi] - z0, field.x[pj] - x0,
                                    field.y[pj] - y0, field.z[pj] - z0) +
             detail::angle_skew_deg(field.x[pi] - x0, field.y[pi] - y0,
                                    field.z[pi] - z0, field.x[pk] - x0,
                                    field.y[pk] - y0, field.z[pk] - z0) +
             detail::angle_skew_deg(field.x[pj] - x0, field.y[pj] - y0,
                                    field.z[pj] - z0, field.x[pk] - x0,
                                    field.y[pk] - y0, field.z[pk] - z0)) /
            3.0f;
        out.cell_delta[static_cast<size_t>(k * cy * cx + j * cx + i)] = s;
      }
    }
  }
  return out;
}

bool sample_orthogonality_raster(NodeField2d field,
                                 const float* node_delta,
                                 int raster_w,
                                 int raster_h,
                                 std::vector<float>* out_delta,
                                 double* min_x,
                                 double* min_y,
                                 double* max_x,
                                 double* max_y) {
  if (!out_delta || !node_delta || !detail::field2d_shaped(field) ||
      raster_w < 2 || raster_h < 2 || !min_x || !min_y || !max_x || !max_y) {
    return false;
  }
  const int n = field.nx * field.ny;
  double lo_x = field.x[0];
  double hi_x = field.x[0];
  double lo_y = field.y[0];
  double hi_y = field.y[0];
  for (int k = 1; k < n; ++k) {
    lo_x = std::min(lo_x, field.x[k]);
    hi_x = std::max(hi_x, field.x[k]);
    lo_y = std::min(lo_y, field.y[k]);
    hi_y = std::max(hi_y, field.y[k]);
  }
  if (!(hi_x > lo_x) || !(hi_y > lo_y)) {
    return false;
  }
  *min_x = lo_x;
  *min_y = lo_y;
  *max_x = hi_x;
  *max_y = hi_y;
  out_delta->assign(static_cast<size_t>(raster_w * raster_h), 0.f);
  const double dx = (hi_x - lo_x) / static_cast<double>(raster_w);
  const double dy = (hi_y - lo_y) / static_cast<double>(raster_h);
  const double inv_i = 1.0 / static_cast<double>(field.nx - 1);
  const double inv_j = 1.0 / static_cast<double>(field.ny - 1);
  for (int rj = 0; rj < raster_h; ++rj) {
    for (int ri = 0; ri < raster_w; ++ri) {
      const double x = lo_x + (static_cast<double>(ri) + 0.5) * dx;
      const double y = lo_y + (static_cast<double>(rj) + 0.5) * dy;
      const double u = (x - lo_x) / (hi_x - lo_x);
      const double v = (y - lo_y) / (hi_y - lo_y);
      const double fi = std::max(
          0.0, std::min(static_cast<double>(field.nx - 1), u / inv_i));
      const double fj = std::max(
          0.0, std::min(static_cast<double>(field.ny - 1), v / inv_j));
      const int i0 = static_cast<int>(fi);
      const int j0 = static_cast<int>(fj);
      const int i1 = std::min(i0 + 1, field.nx - 1);
      const int j1 = std::min(j0 + 1, field.ny - 1);
      const float fx = static_cast<float>(fi - static_cast<double>(i0));
      const float fy = static_cast<float>(fj - static_cast<double>(j0));
      const float a = node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i0, j0))];
      const float b = node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i1, j0))];
      const float c = node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i0, j1))];
      const float d = node_delta[static_cast<size_t>(
          detail::node_index_2d(field.nx, i1, j1))];
      const float ab = a + (b - a) * fx;
      const float cd = c + (d - c) * fx;
      (*out_delta)[static_cast<size_t>(rj * raster_w + ri)] =
          ab + (cd - ab) * fy;
    }
  }
  return true;
}

}  // namespace geo

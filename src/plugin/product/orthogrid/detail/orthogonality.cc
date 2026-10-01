// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid/detail/orthogonality.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

#include <Eigen/Core>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace orthogrid {
namespace {

constexpr float kEps = 1e-18f;

int node_index(int nx, int i, int j) {
  return j * nx + i;
}

}  // namespace

const char* heat_class_from_delta(float delta_deg) {
  if (delta_deg < 5.f) {
    return "0";
  }
  if (delta_deg < 15.f) {
    return "1";
  }
  if (delta_deg < 30.f) {
    return "2";
  }
  return "3";
}

const char* format_heat_delta(float delta_deg, char* buf, size_t cap) {
  if (!buf || cap < 4) {
    return "";
  }
  float v = delta_deg;
  if (!(v >= 0.f)) {
    v = 0.f;
  }
  if (v > 90.f) {
    v = 90.f;
  }
  const int n = std::snprintf(buf, cap, "%.4f", static_cast<double>(v));
  if (n <= 0 || static_cast<size_t>(n) >= cap) {
    buf[0] = '\0';
    return "";
  }
  return buf;
}

OrthogonalityField compute_orthogonality(const GridField& grid) {
  OrthogonalityField out;
  if (!grid.is_shaped()) {
    return out;
  }
  const size_t n = static_cast<size_t>(grid.nx * grid.ny);
  out.node_delta.assign(n, 0.f);
  for (int j = 1; j < grid.ny - 1; ++j) {
    for (int i = 1; i < grid.nx - 1; ++i) {
      const Eigen::Vector2d half_ksi(
          0.5 * (grid.x(j, i + 1) - grid.x(j, i - 1)),
          0.5 * (grid.y(j, i + 1) - grid.y(j, i - 1)));
      const Eigen::Vector2d half_eta(
          0.5 * (grid.x(j + 1, i) - grid.x(j - 1, i)),
          0.5 * (grid.y(j + 1, i) - grid.y(j - 1, i)));
      const double arfa = half_eta.squaredNorm();
      const double gama = half_ksi.squaredNorm();
      const double beta = half_ksi.dot(half_eta);
      const double denom = std::sqrt(arfa * gama);
      float delta = 90.f;
      if (denom > static_cast<double>(kEps)) {
        double c = beta / denom;
        c = std::max(-1.0, std::min(1.0, c));
        const float theta =
            static_cast<float>(std::acos(c) * 180.0 / M_PI);
        delta = std::fabs(90.f - theta);
      }
      out.node_delta[static_cast<size_t>(node_index(grid.nx, i, j))] = delta;
    }
  }

  const int cx = grid.nx - 1;
  const int cy = grid.ny - 1;
  out.cell_delta.assign(static_cast<size_t>(cx * cy), 0.f);
  for (int j = 0; j < cy; ++j) {
    for (int i = 0; i < cx; ++i) {
      const float a =
          out.node_delta[static_cast<size_t>(node_index(grid.nx, i, j))];
      const float b =
          out.node_delta[static_cast<size_t>(node_index(grid.nx, i + 1, j))];
      const float c =
          out.node_delta[static_cast<size_t>(node_index(grid.nx, i + 1, j + 1))];
      const float d =
          out.node_delta[static_cast<size_t>(node_index(grid.nx, i, j + 1))];
      out.cell_delta[static_cast<size_t>(j * cx + i)] = (a + b + c + d) * 0.25f;
    }
  }
  return out;
}

bool sample_orthogonality_raster(const GridField& grid,
                                 const float* node_delta,
                                 int raster_w,
                                 int raster_h,
                                 std::vector<float>* out_delta,
                                 double* min_x,
                                 double* min_y,
                                 double* max_x,
                                 double* max_y) {
  if (!out_delta || !node_delta || !grid.is_shaped() || raster_w < 2 ||
      raster_h < 2 || !min_x || !min_y || !max_x || !max_y) {
    return false;
  }
  const double lo_x = grid.x.minCoeff();
  const double hi_x = grid.x.maxCoeff();
  const double lo_y = grid.y.minCoeff();
  const double hi_y = grid.y.maxCoeff();
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
  const double inv_i = 1.0 / static_cast<double>(grid.nx - 1);
  const double inv_j = 1.0 / static_cast<double>(grid.ny - 1);
  for (int rj = 0; rj < raster_h; ++rj) {
    for (int ri = 0; ri < raster_w; ++ri) {
      const double x = lo_x + (static_cast<double>(ri) + 0.5) * dx;
      const double y = lo_y + (static_cast<double>(rj) + 0.5) * dy;
      const double u = (x - lo_x) / (hi_x - lo_x);
      const double v = (y - lo_y) / (hi_y - lo_y);
      const double fi = std::max(
          0.0, std::min(static_cast<double>(grid.nx - 1), u / inv_i));
      const double fj = std::max(
          0.0, std::min(static_cast<double>(grid.ny - 1), v / inv_j));
      const int i0 = static_cast<int>(fi);
      const int j0 = static_cast<int>(fj);
      const int i1 = std::min(i0 + 1, grid.nx - 1);
      const int j1 = std::min(j0 + 1, grid.ny - 1);
      const float fx = static_cast<float>(fi - static_cast<double>(i0));
      const float fy = static_cast<float>(fj - static_cast<double>(j0));
      const float a =
          node_delta[static_cast<size_t>(node_index(grid.nx, i0, j0))];
      const float b =
          node_delta[static_cast<size_t>(node_index(grid.nx, i1, j0))];
      const float c =
          node_delta[static_cast<size_t>(node_index(grid.nx, i0, j1))];
      const float d =
          node_delta[static_cast<size_t>(node_index(grid.nx, i1, j1))];
      const float ab = a + (b - a) * fx;
      const float cd = c + (d - c) * fx;
      (*out_delta)[static_cast<size_t>(rj * raster_w + ri)] =
          ab + (cd - ab) * fy;
    }
  }
  return true;
}

}  // namespace orthogrid

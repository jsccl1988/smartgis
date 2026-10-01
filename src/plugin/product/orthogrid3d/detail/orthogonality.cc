// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid3d/detail/orthogonality.h"

#include <cmath>

namespace orthogrid3d {
namespace detail {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;

struct Vec3 {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

Vec3 sub(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

double length(const Vec3& v) {
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

float angle_skew_deg(const Vec3& a, const Vec3& b) {
  const double la = length(a);
  const double lb = length(b);
  if (la < 1e-18 || lb < 1e-18) {
    return 90.0f;
  }
  double c = dot(a, b) / (la * lb);
  if (c < -1.0) {
    c = -1.0;
  } else if (c > 1.0) {
    c = 1.0;
  }
  const double theta_deg = std::acos(c) * kRadToDeg;
  return static_cast<float>(std::fabs(90.0 - theta_deg));
}

namespace {

int node_index(int nx, int ny, int i, int j, int k) {
  return k * ny * nx + j * nx + i;
}

}  // namespace

Vec3 at(const VolumeField& field, int i, int j, int k) {
  const int idx = node_index(field.nx, field.ny, i, j, k);
  return {field.x[idx], field.y[idx], field.z[idx]};
}

}  // namespace detail

OrthogonalityField3d compute_orthogonality_3d(const VolumeField& field) {
  OrthogonalityField3d out;
  if (field.nx < 2 || field.ny < 2 || field.nz < 2 || field.x == nullptr ||
      field.y == nullptr || field.z == nullptr) {
    return out;
  }
  const int cx = field.nx - 1;
  const int cy = field.ny - 1;
  const int cz = field.nz - 1;
  out.cell_delta.assign(static_cast<size_t>(cx * cy * cz), 0.0f);

  for (int k = 0; k < cz; ++k) {
    for (int j = 0; j < cy; ++j) {
      for (int i = 0; i < cx; ++i) {
        using detail::Vec3;
        const Vec3 p000 = detail::at(field, i, j, k);
        const Vec3 ei = detail::sub(detail::at(field, i + 1, j, k), p000);
        const Vec3 ej = detail::sub(detail::at(field, i, j + 1, k), p000);
        const Vec3 ek = detail::sub(detail::at(field, i, j, k + 1), p000);
        const float s =
            (detail::angle_skew_deg(ei, ej) + detail::angle_skew_deg(ei, ek) +
             detail::angle_skew_deg(ej, ek)) /
            3.0f;
        const int cidx = k * cy * cx + j * cx + i;
        out.cell_delta[static_cast<size_t>(cidx)] = s;
      }
    }
  }
  return out;
}

}  // namespace orthogrid3d

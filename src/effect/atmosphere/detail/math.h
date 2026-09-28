// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_DETAIL_MATH_H_
#define EFFECT_ATMOSPHERE_DETAIL_MATH_H_

#include <algorithm>
#include <cmath>
#include <vector>

namespace effect {
namespace atmosphere {
namespace detail {

inline float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

inline float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

inline void normalize3(float* x, float* y, float* z) {
  const float len = std::sqrt((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
  if (len > 1.0e-6f) {
    *x /= len;
    *y /= len;
    *z /= len;
  } else {
    *x = 0.0f;
    *y = 1.0f;
    *z = 0.0f;
  }
}

inline void sun_from_azimuth_elevation(float azimuth_rad, float elevation_rad,
                                       float* x, float* y, float* z) {
  const float ce = std::cos(elevation_rad);
  *x = std::sin(azimuth_rad) * ce;
  *y = std::sin(elevation_rad);
  *z = std::cos(azimuth_rad) * ce;
  normalize3(x, y, z);
}

// Negated column-major view translation (elements 12..14) as a world-space eye.
inline void eye_from_view(const float view[16], float* x, float* y, float* z) {
  *x = -view[12];
  *y = -view[13];
  *z = -view[14];
}

inline float sample_bilinear(const std::vector<float>& grid, int cols, int rows,
                             float u, float v) {
  if (grid.empty() || cols < 1 || rows < 1) {
    return 1.0f;
  }
  u = clampf(u, 0.0f, 1.0f);
  v = clampf(v, 0.0f, 1.0f);
  const float x = u * static_cast<float>(cols - 1);
  const float y = v * static_cast<float>(rows - 1);
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, cols - 1);
  const int y1 = std::min(y0 + 1, rows - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float a = grid[static_cast<std::size_t>(y0 * cols + x0)];
  const float b = grid[static_cast<std::size_t>(y0 * cols + x1)];
  const float c = grid[static_cast<std::size_t>(y1 * cols + x0)];
  const float d = grid[static_cast<std::size_t>(y1 * cols + x1)];
  const float ab = a * (1.0f - tx) + b * tx;
  const float cd = c * (1.0f - tx) + d * tx;
  return ab * (1.0f - ty) + cd * ty;
}

}  // namespace detail
}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_DETAIL_MATH_H_

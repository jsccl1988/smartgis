// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/contour/msquares.h"

#include <algorithm>
#include <cmath>

namespace vista {
namespace detail {
namespace {

float clampf(float v, float lo, float hi) {
  return (std::max)(lo, (std::min)(hi, v));
}

float edge_t(float ha, float hb, float z) {
  const float d = hb - ha;
  if (std::fabs(d) < 1e-6f) {
    return 0.5f;
  }
  return clampf((z - ha) / d, 0.f, 1.f);
}

}  // namespace

void cell_segments(float h00, float h10, float h11, float h01, float z,
                   float c, float r, std::vector<float>* xy) {
  // Corners: 00 SW, 10 SE, 11 NE, 01 NW. Bit = above isolevel.
  const int mask = (h00 >= z ? 1 : 0) | (h10 >= z ? 2 : 0) |
                   (h11 >= z ? 4 : 0) | (h01 >= z ? 8 : 0);
  if (mask == 0 || mask == 15) {
    return;
  }
  const float t_s = edge_t(h00, h10, z);
  const float t_e = edge_t(h10, h11, z);
  const float t_n = edge_t(h01, h11, z);
  const float t_w = edge_t(h00, h01, z);
  const float sx = c + t_s;
  const float sy = r;
  const float ex = c + 1.f;
  const float ey = r + t_e;
  const float nx = c + t_n;
  const float ny = r + 1.f;
  const float wx = c;
  const float wy = r + t_w;
  auto emit = [&](float x0, float y0, float x1, float y1) {
    xy->push_back(x0);
    xy->push_back(y0);
    xy->push_back(x1);
    xy->push_back(y1);
  };
  switch (mask) {
    case 1:
    case 14:
      emit(sx, sy, wx, wy);
      break;
    case 2:
    case 13:
      emit(sx, sy, ex, ey);
      break;
    case 3:
    case 12:
      emit(wx, wy, ex, ey);
      break;
    case 4:
    case 11:
      emit(ex, ey, nx, ny);
      break;
    case 5:
      emit(sx, sy, wx, wy);
      emit(ex, ey, nx, ny);
      break;
    case 6:
    case 9:
      emit(sx, sy, nx, ny);
      break;
    case 7:
    case 8:
      emit(wx, wy, nx, ny);
      break;
    case 10:
      emit(sx, sy, ex, ey);
      emit(wx, wy, nx, ny);
      break;
    default:
      break;
  }
}

}  // namespace detail
}  // namespace vista

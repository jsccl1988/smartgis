// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/terrain/process/dem_hillshade.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace vista {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float clampf(float v, float lo, float hi) {
  return (std::max)(lo, (std::min)(hi, v));
}

float deg_to_rad(float deg) { return deg * (kPi / 180.f); }

void unpack_rgb(uint32_t argb, float* r, float* g, float* b) {
  *r = static_cast<float>((argb >> 16) & 0xff) / 255.f;
  *g = static_cast<float>((argb >> 8) & 0xff) / 255.f;
  *b = static_cast<float>(argb & 0xff) / 255.f;
}

}  // namespace

bool shade_dem_rgba(const DemRaster& dem, const HillshadeParams& params,
                    std::vector<uint8_t>* rgba, int* out_w, int* out_h) {
  if (!rgba || dem.empty()) {
    return false;
  }
  const int cols = dem.cols();
  const int rows = dem.rows();
  int step_x = 1;
  int step_y = 1;
  if (params.max_edge >= 8) {
    if (cols > params.max_edge) {
      step_x = cols / params.max_edge;
    }
    if (rows > params.max_edge) {
      step_y = rows / params.max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols + step_x - 1) / step_x;
  const int h = (rows + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }

  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  dem.envelope(&minx, &miny, &maxx, &maxy);
  // Envelope is geographic degrees; convert cell size to meters so slope
  // uses consistent units with elevation samples (meters).
  const float cell_x_deg =
      static_cast<float>((maxx - minx) / static_cast<double>((std::max)(1, cols - 1)));
  const float cell_y_deg =
      static_cast<float>((maxy - miny) / static_cast<double>((std::max)(1, rows - 1)));
  const float cell_x_m = (std::max)(1.f, cell_x_deg * 111320.f);
  const float cell_y_m = (std::max)(1.f, cell_y_deg * 111320.f);
  const float exag = clampf(params.exaggeration, 0.01f, 8.f);

  const float az = deg_to_rad(params.illumination_direction_deg);
  const float alt = deg_to_rad(params.illumination_altitude_deg);
  const float cos_alt = std::cos(alt);
  const float sin_alt = std::sin(alt);

  float sr = 0;
  float sg = 0;
  float sb = 0;
  float hr = 1;
  float hg = 1;
  float hb = 1;
  unpack_rgb(params.shadow_argb, &sr, &sg, &sb);
  unpack_rgb(params.highlight_argb, &hr, &hg, &hb);

  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);

  auto sample_m = [&](int col, int row) -> float {
    col = (std::max)(0, (std::min)(cols - 1, col));
    row = (std::max)(0, (std::min)(rows - 1, row));
    // Geographic sample via envelope fractions (DemRaster::sample uses x,y).
    const double x =
        minx + (maxx - minx) * (static_cast<double>(col) /
                                static_cast<double>((std::max)(1, cols - 1)));
    const double y =
        maxy - (maxy - miny) * (static_cast<double>(row) /
                                static_cast<double>((std::max)(1, rows - 1)));
    return dem.sample_meters(x, y);
  };

  for (int row = 0; row < h; ++row) {
    const int src_row = (std::min)(rows - 1, row * step_y);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols - 1, col * step_x);
      const float c = sample_m(src_col, src_row);
      // Treat near-flat ocean / nodata as transparent.
      if (c <= 1.f) {
        continue;
      }
      const float zw = sample_m(src_col - step_x, src_row);
      const float ze = sample_m(src_col + step_x, src_row);
      const float zs = sample_m(src_col, src_row + step_y);
      const float zn = sample_m(src_col, src_row - step_y);
      const float dx_m = 2.f * static_cast<float>(step_x) * cell_x_m;
      const float dy_m = 2.f * static_cast<float>(step_y) * cell_y_m;
      const float sx = -((ze - zw) / dx_m) * exag;
      const float sy = -((zn - zs) / dy_m) * exag;
      const float slope = std::atan(std::sqrt(sx * sx + sy * sy));
      float aspect = 0.f;
      if (sx != 0.f || sy != 0.f) {
        aspect = std::atan2(sy, -sx);
      }
      float shade =
          sin_alt * std::cos(slope) +
          cos_alt * std::sin(slope) * std::cos(az - aspect);
      shade = clampf(shade, 0.f, 1.f);
      // Sharper contrast: deeper umbra / brighter lit facets so DEM relief
      // reads crisp under soft-multiply. Floor stays above crushing cream
      // land out of the map2d_china land_cream gate (r>170 after opacity).
      shade = clampf((shade - 0.5f) * 1.80f + 0.5f, 0.08f, 1.f);
      const float t = shade;
      const float r = sr + (hr - sr) * t;
      const float g = sg + (hg - sg) * t;
      const float b = sb + (hb - sb) * t;
      // Opaque land only — coast fringe is discarded at blit time so bake
      // does not pre-soften into a cast-shadow rim.
      const float edge_a = 1.f;
      const size_t i =
          (static_cast<size_t>(row) * static_cast<size_t>(w) +
           static_cast<size_t>(col)) *
          4u;
      (*rgba)[i + 0] = static_cast<uint8_t>(clampf(r, 0.f, 1.f) * 255.f + 0.5f);
      (*rgba)[i + 1] = static_cast<uint8_t>(clampf(g, 0.f, 1.f) * 255.f + 0.5f);
      (*rgba)[i + 2] = static_cast<uint8_t>(clampf(b, 0.f, 1.f) * 255.f + 0.5f);
      (*rgba)[i + 3] =
          static_cast<uint8_t>(clampf(edge_a, 0.f, 1.f) * 255.f + 0.5f);
    }
  }

  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  return true;
}

}  // namespace vista

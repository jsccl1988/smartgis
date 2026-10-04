// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/process/dem_hillshade.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "base/math/scalar/constants.h"
#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "gis/analysis/raster/dem/hillshade.h"
#include "vista/terrain/process/bake_backend.h"
#include "vista/terrain/process/nv/thrust_gis.h"

namespace {

std::atomic<int> g_last_shade_cuda{0};

}  // namespace

namespace vista {
namespace {

constexpr int kParallelShadeMinRows = 8;
constexpr int kParallelShadeMinPixels = 4096;

float unpack_u8(uint32_t argb, int shift) {
  return static_cast<float>((argb >> shift) & 0xff) / 255.f;
}

BakeBackend shade_backend() {
  if (const char* s = base::switch_cstr("bake-backend"); s && s[0]) {
    return parse_bake_backend_token(s);
  }
  return bake_backend_from_env();
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
  const float exag = std::clamp(params.exaggeration, 0.01f, 8.f);

  const float az = base::deg_to_rad(params.illumination_direction_deg);
  const float alt = base::deg_to_rad(params.illumination_altitude_deg);
  const float cos_alt = std::cos(alt);
  const float sin_alt = std::sin(alt);
  const float dx_m = 2.f * static_cast<float>(step_x) * cell_x_m;
  const float dy_m = 2.f * static_cast<float>(step_y) * cell_y_m;

  const float sr = unpack_u8(params.shadow_argb, 16);
  const float sg = unpack_u8(params.shadow_argb, 8);
  const float sb = unpack_u8(params.shadow_argb, 0);
  const float hr = unpack_u8(params.highlight_argb, 16);
  const float hg = unpack_u8(params.highlight_argb, 8);
  const float hb = unpack_u8(params.highlight_argb, 0);

  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  uint8_t* pixels = rgba->data();
  g_last_shade_cuda.store(0, std::memory_order_relaxed);

  const BakeBackend backend = shade_backend();
  const bool allow_cuda = backend != BakeBackend::kCpu;
  const bool require_cuda = backend == BakeBackend::kCuda;

  const std::vector<float>* heights = nullptr;
  dem.export_bake_cache(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr, &heights, nullptr);
  if (allow_cuda && heights &&
      heights->size() ==
          static_cast<size_t>(cols) * static_cast<size_t>(rows)) {
    BASE_TRACE_EVENT("shade_cuda", "bake");
    if (try_shade_dem_thrust(heights->data(), cols, rows, step_x, step_y, w, h,
                             dx_m, dy_m, exag, az, sin_alt, cos_alt, sr, sg, sb,
                             hr, hg, hb, pixels)) {
      g_last_shade_cuda.store(1, std::memory_order_relaxed);
      if (out_w) {
        *out_w = w;
      }
      if (out_h) {
        *out_h = h;
      }
      return true;
    }
  }
  if (require_cuda) {
    return false;
  }

  const bool grid_ok =
      heights &&
      heights->size() == static_cast<size_t>(cols) * static_cast<size_t>(rows);
  std::vector<float> shade_grid;
  int shade_w = 0;
  int shade_h = 0;
  const bool have_grid =
      grid_ok &&
      gis::detail::horn_lambert_shade_grid(
          heights->data(), cols, rows, step_x, step_y, dx_m, dy_m, exag, az,
          sin_alt, cos_alt, &shade_grid, &shade_w, &shade_h) &&
      shade_w == w && shade_h == h;

  auto shade_row = [&](int row) {
    const int src_row = (std::min)(rows - 1, row * step_y);
    uint8_t* rowp =
        pixels +
        static_cast<size_t>(row) * static_cast<size_t>(w) * 4u;
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols - 1, col * step_x);
      const float c = dem.meters_at(src_col, src_row);
      // Treat near-flat ocean / nodata as transparent.
      if (c <= 1.f) {
        continue;
      }
      float shade = 0.f;
      if (have_grid) {
        shade = shade_grid[static_cast<size_t>(row) * static_cast<size_t>(w) +
                           static_cast<size_t>(col)];
      } else {
        const float zw = dem.meters_at(src_col - step_x, src_row);
        const float ze = dem.meters_at(src_col + step_x, src_row);
        const float zs = dem.meters_at(src_col, src_row + step_y);
        const float zn = dem.meters_at(src_col, src_row - step_y);
        shade = gis::detail::horn_lambert_shade(zw, ze, zs, zn, dx_m, dy_m, exag,
                                               az, sin_alt, cos_alt);
      }
      // Sharper contrast: deeper umbra / brighter lit facets so DEM relief
      // reads crisp under soft-multiply. Floor stays above crushing cream
      // land out of the map2d_china land_cream gate (r>170 after opacity).
      shade = std::clamp((shade - 0.5f) * 1.80f + 0.5f, 0.08f, 1.f);
      const size_t i = static_cast<size_t>(col) * 4u;
      rowp[i + 0] =
          static_cast<uint8_t>(std::clamp(sr + (hr - sr) * shade, 0.f, 1.f) *
                                   255.f +
                               0.5f);
      rowp[i + 1] =
          static_cast<uint8_t>(std::clamp(sg + (hg - sg) * shade, 0.f, 1.f) *
                                   255.f +
                               0.5f);
      rowp[i + 2] =
          static_cast<uint8_t>(std::clamp(sb + (hb - sb) * shade, 0.f, 1.f) *
                                   255.f +
                               0.5f);
      rowp[i + 3] = 255;
    }
  };

  {
    BASE_TRACE_EVENT("shade_cpu", "bake");
    if (h >= kParallelShadeMinRows && w * h >= kParallelShadeMinPixels) {
      base::execution::GlobalNThreadPoolExecutor executor;
      base::execution::parallel_for(executor, 0, h, shade_row);
    } else {
      for (int row = 0; row < h; ++row) {
        shade_row(row);
      }
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

int last_shade_used_cuda() {
  return g_last_shade_cuda.load(std::memory_order_relaxed);
}

void reset_last_shade_used_cuda() {
  g_last_shade_cuda.store(0, std::memory_order_relaxed);
}

}  // namespace vista

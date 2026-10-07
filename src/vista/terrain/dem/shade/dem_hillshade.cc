// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/shade/dem_hillshade.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "base/math/scalar/constants.h"
#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "gis/analysis/raster/dem/hillshade.h"
#include "vista/terrain/dem/bake/bake_backend.h"
#include "vista/terrain/dem/bake/bake_parallel.h"
#include "vista/terrain/dem/dem_contour.h"
#include "vista/terrain/dem/detail/dem_simd.h"
#include "vista/terrain/dem/nv/thrust_gis.h"
#include "vista/terrain/dem/shade/lit_kern.h"

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

float encode_shade(float shade) {
  return std::clamp((shade - 0.5f) * 1.80f + 0.5f, 0.08f, 1.f);
}

void write_lambert_rgba(uint8_t* px, float shade, bool contrast, float sr,
                        float sg, float sb, float hr, float hg, float hb) {
  if (contrast) {
    shade = encode_shade(shade);
  } else {
    shade = std::clamp(shade, 0.08f, 1.f);
  }
  px[0] = static_cast<uint8_t>(
      std::clamp(sr + (hr - sr) * shade, 0.f, 1.f) * 255.f + 0.5f);
  px[1] = static_cast<uint8_t>(
      std::clamp(sg + (hg - sg) * shade, 0.f, 1.f) * 255.f + 0.5f);
  px[2] = static_cast<uint8_t>(
      std::clamp(sb + (hb - sb) * shade, 0.f, 1.f) * 255.f + 0.5f);
  px[3] = 255;
}

// Bilinear sample of the DEM grid at fractional column/row (clamped).
float sample_meters_bilinear(const DemRaster& dem, float col_f, float row_f) {
  const int cols = dem.cols();
  const int rows = dem.rows();
  if (cols < 1 || rows < 1) {
    return 0.f;
  }
  col_f = std::clamp(col_f, 0.f, static_cast<float>(cols - 1));
  row_f = std::clamp(row_f, 0.f, static_cast<float>(rows - 1));
  const int c0 = static_cast<int>(std::floor(col_f));
  const int r0 = static_cast<int>(std::floor(row_f));
  const int c1 = (std::min)(c0 + 1, cols - 1);
  const int r1 = (std::min)(r0 + 1, rows - 1);
  const float tx = col_f - static_cast<float>(c0);
  const float ty = row_f - static_cast<float>(r0);
  const float h00 = dem.meters_at(c0, r0);
  const float h10 = dem.meters_at(c1, r0);
  const float h01 = dem.meters_at(c0, r1);
  const float h11 = dem.meters_at(c1, r1);
  const float h0 = h00 * (1.f - tx) + h10 * tx;
  const float h1 = h01 * (1.f - tx) + h11 * tx;
  return h0 * (1.f - ty) + h1 * ty;
}

// Viewport LOD heights via pixel-center bilinear (avoids nearest-step jaggies).
// Prefers parallel_for + AVX2 over the contiguous height grid when available.
void fill_lod_bilinear(const DemRaster& dem, int w, int h,
                       std::vector<float>* lod) {
  lod->assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0.f);
  if (const float* heights = dem.heights_data()) {
    detail::fill_lod_bilinear_grid(heights, dem.cols(), dem.rows(), w, h,
                                   lod->data());
    return;
  }
  const float cols_f = static_cast<float>(dem.cols());
  const float rows_f = static_cast<float>(dem.rows());
  const float inv_w = 1.f / static_cast<float>(w);
  const float inv_h = 1.f / static_cast<float>(h);
  auto fill_row = [&](int row) {
    const float fy =
        (static_cast<float>(row) + 0.5f) * inv_h * rows_f - 0.5f;
    float* rowp =
        lod->data() + static_cast<size_t>(row) * static_cast<size_t>(w);
    for (int col = 0; col < w; ++col) {
      const float fx =
          (static_cast<float>(col) + 0.5f) * inv_w * cols_f - 0.5f;
      rowp[col] = sample_meters_bilinear(dem, fx, fy);
    }
  };
  if (bake_rows_should_parallel(w, h)) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, h, fill_row);
  } else {
    for (int row = 0; row < h; ++row) {
      fill_row(row);
    }
  }
}

// Second surface: Origin jet sheet + white isolines, lit by the Lambert bake
// already in |pixels|. Ocean stays A=0 so the carto/imagery base shows.
// |lod| is the bilinear-downsampled height grid (w*h).
void compose_elevation_sheet(uint8_t* pixels, int w, int h, const float* lod,
                             float sg, float hg) {
  if (!pixels || !lod || w < 2 || h < 2) {
    return;
  }
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<uint8_t> overlay;
  if (!bake_elevation_overlay_rgba(lod, w, h, true, true, 0.f, &overlay) ||
      overlay.size() < n * 4u) {
    return;
  }
  const float denom = (std::max)(1.e-4f, hg - sg);
  auto is_isoline = [](const uint8_t* ov4) -> bool {
    const int mx = (std::max)(ov4[0], (std::max)(ov4[1], ov4[2]));
    const int mn = (std::min)(ov4[0], (std::min)(ov4[1], ov4[2]));
    // Dark charcoal stamps (paint.cc) or legacy Origin near-white isolines.
    const bool dark = mx < 90 && (mx - mn) < 40;
    const bool light = mx > 210 && (mx - mn) < 48;
    return dark || light;
  };
  for (int row = 0; row < h; ++row) {
    uint8_t* rowp =
        pixels + static_cast<size_t>(row) * static_cast<size_t>(w) * 4u;
    const uint8_t* ov =
        overlay.data() + static_cast<size_t>(row) * static_cast<size_t>(w) * 4u;
    for (int col = 0; col < w; ++col) {
      const size_t i = static_cast<size_t>(col) * 4u;
      if (ov[i + 3] == 0) {
        rowp[i + 0] = 0;
        rowp[i + 1] = 0;
        rowp[i + 2] = 0;
        rowp[i + 3] = 0;
        continue;
      }
      if (is_isoline(ov + i)) {
        rowp[i + 0] = ov[i + 0];
        rowp[i + 1] = ov[i + 1];
        rowp[i + 2] = ov[i + 2];
        rowp[i + 3] = 255;
        continue;
      }
      const float shade =
          (static_cast<float>(rowp[i + 1]) / 255.f - sg) / denom;
      const float lit = 0.28f + 0.72f * std::clamp(shade, 0.08f, 1.f);
      rowp[i + 0] = static_cast<uint8_t>(
          std::clamp(static_cast<float>(ov[i + 0]) * lit, 0.f, 255.f) + 0.5f);
      rowp[i + 1] = static_cast<uint8_t>(
          std::clamp(static_cast<float>(ov[i + 1]) * lit, 0.f, 255.f) + 0.5f);
      rowp[i + 2] = static_cast<uint8_t>(
          std::clamp(static_cast<float>(ov[i + 2]) * lit, 0.f, 255.f) + 0.5f);
      rowp[i + 3] = 255;
    }
  }
  // Soft-feather isoline edges into neighboring jet cells (1px) so upscale
  // bilinear has a coverage ramp instead of a hard white/stair edge.
  for (int row = 1; row + 1 < h; ++row) {
    for (int col = 1; col + 1 < w; ++col) {
      const size_t i =
          (static_cast<size_t>(row) * static_cast<size_t>(w) +
           static_cast<size_t>(col)) *
          4u;
      const uint8_t* ov = overlay.data() + i;
      if (ov[3] == 0 || is_isoline(ov)) {
        continue;
      }
      int iso_n = 0;
      const int dcol[4] = {-1, 1, 0, 0};
      const int drow[4] = {0, 0, -1, 1};
      for (int k = 0; k < 4; ++k) {
        const size_t j =
            (static_cast<size_t>(row + drow[k]) * static_cast<size_t>(w) +
             static_cast<size_t>(col + dcol[k])) *
            4u;
        if (overlay[j + 3] != 0 && is_isoline(overlay.data() + j)) {
          ++iso_n;
        }
      }
      if (iso_n == 0) {
        continue;
      }
      uint8_t* rowp = pixels + i;
      const float t = 0.22f * static_cast<float>(iso_n);
      // Soft-feather toward dark charcoal isoline (matches paint.cc stamps).
      constexpr float kIsoR = 28.f;
      constexpr float kIsoG = 34.f;
      constexpr float kIsoB = 52.f;
      rowp[0] = static_cast<uint8_t>(
          std::clamp(static_cast<float>(rowp[0]) * (1.f - t) + kIsoR * t, 0.f,
                     255.f) +
          0.5f);
      rowp[1] = static_cast<uint8_t>(
          std::clamp(static_cast<float>(rowp[1]) * (1.f - t) + kIsoG * t, 0.f,
                     255.f) +
          0.5f);
      rowp[2] = static_cast<uint8_t>(
          std::clamp(static_cast<float>(rowp[2]) * (1.f - t) + kIsoB * t, 0.f,
                     255.f) +
          0.5f);
    }
  }
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

  // Bilinear LOD first, then shade on the dense w×h grid (step=1). Nearest
  // step sampling left staircase bands and isolines after zoom stretch.
  std::vector<float> lod;
  {
    BASE_TRACE_EVENT("lod_bilinear", "bake");
    fill_lod_bilinear(dem, w, h, &lod);
  }

  const BakeBackend backend = shade_backend();
  const bool allow_cuda = backend != BakeBackend::kCpu;
  const bool require_cuda = backend == BakeBackend::kCuda;

  if (allow_cuda) {
    BASE_TRACE_EVENT("shade_cuda", "bake");
    if (try_shade_dem_thrust(lod.data(), w, h, 1, 1, w, h, dx_m, dy_m, exag, az,
                             sin_alt, cos_alt, sr, sg, sb, hr, hg, hb, pixels)) {
      g_last_shade_cuda.store(1, std::memory_order_relaxed);
      if (params.color_ramp == 1) {
        compose_elevation_sheet(pixels, w, h, lod.data(), sg, hg);
      }
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

  std::vector<float> shade_grid;
  int shade_w = 0;
  int shade_h = 0;
  const bool have_grid =
      gis::detail::horn_lambert_shade_grid(
          lod.data(), w, h, 1, 1, dx_m, dy_m, exag, az, sin_alt, cos_alt,
          &shade_grid, &shade_w, &shade_h) &&
      shade_w == w && shade_h == h;

  {
    BASE_TRACE_EVENT("shade_cpu", "bake");
    if (have_grid) {
      // AVX2 / scalar Lambert pack over the Horn grid (ocean via lod <= 1).
      detail::pack_lambert_from_shade(shade_grid.data(), lod.data(), pixels,
                                      static_cast<size_t>(w) *
                                          static_cast<size_t>(h),
                                      sr, sg, sb, hr, hg, hb, /*contrast=*/true);
    } else {
      auto shade_row = [&](int row) {
        uint8_t* rowp =
            pixels +
            static_cast<size_t>(row) * static_cast<size_t>(w) * 4u;
        const float* lod_row =
            lod.data() + static_cast<size_t>(row) * static_cast<size_t>(w);
        for (int col = 0; col < w; ++col) {
          const float c = lod_row[col];
          if (c <= 1.f) {
            continue;
          }
          const int c0 = (std::max)(0, col - 1);
          const int c1 = (std::min)(w - 1, col + 1);
          const int r0 = (std::max)(0, row - 1);
          const int r1 = (std::min)(h - 1, row + 1);
          const float zw =
              lod[static_cast<size_t>(row) * static_cast<size_t>(w) +
                  static_cast<size_t>(c0)];
          const float ze =
              lod[static_cast<size_t>(row) * static_cast<size_t>(w) +
                  static_cast<size_t>(c1)];
          const float zs =
              lod[static_cast<size_t>(r1) * static_cast<size_t>(w) +
                  static_cast<size_t>(col)];
          const float zn =
              lod[static_cast<size_t>(r0) * static_cast<size_t>(w) +
                  static_cast<size_t>(col)];
          const float shade = gis::detail::horn_lambert_shade(
              zw, ze, zs, zn, dx_m, dy_m, exag, az, sin_alt, cos_alt);
          write_lambert_rgba(rowp + static_cast<size_t>(col) * 4u, shade, true,
                             sr, sg, sb, hr, hg, hb);
        }
      };
      if (bake_parallel_wanted() && h >= kParallelShadeMinRows &&
          w * h >= kParallelShadeMinPixels) {
        base::execution::GlobalNThreadPoolExecutor executor;
        base::execution::parallel_for(executor, 0, h, shade_row);
      } else {
        for (int row = 0; row < h; ++row) {
          shade_row(row);
        }
      }
    }
  }

  if (params.color_ramp == 1) {
    compose_elevation_sheet(pixels, w, h, lod.data(), sg, hg);
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

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/terrain/dem/raster/dem_raster.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/time/elapsed_timer.h"
#include "vista/terrain/dem/dem_bake_cache.h"
#include "vista/terrain/dem/bake/bake_parallel.h"
#include "vista/terrain/dem/nv/bake_pixel.h"
#include "vista/terrain/dem/nv/thrust_gis.h"
#include "vista/terrain/dem/raster/bake_util.h"

namespace vista {

bool DemRaster::bake_hypsometric_rgba(int max_edge, std::vector<uint8_t>* rgba,
                                      int* out_w, int* out_h) const {
  if (!rgba || empty()) {
    return false;
  }
  base::ElapsedTimer hypso_timer;
  const bool skip_cache = bake_skip_result_cache();
  if (!skip_cache && !source_path_.empty() &&
      dem_hypso_cache_try_get(source_path_.c_str(), max_edge, rgba, out_w,
                              out_h)) {
    note_dem_phase_hypso(
        static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
        /*cache_hit=*/true);
    return !rgba->empty();
  }
  int step_x = 1;
  int step_y = 1;
  if (max_edge >= 8) {
    if (cols_ > max_edge) {
      step_x = cols_ / max_edge;
    }
    if (rows_ > max_edge) {
      step_y = rows_ / max_edge;
    }
  }
  step_x = (std::max)(1, step_x);
  step_y = (std::max)(1, step_y);
  const int w = (cols_ + step_x - 1) / step_x;
  const int h = (rows_ + step_y - 1) / step_y;
  if (w < 2 || h < 2) {
    return false;
  }
  rgba->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
  const BakeBackend backend = resolved_bake_backend();
  const bool allow_cuda = backend != BakeBackend::kCpu;
  const bool require_cuda = backend == BakeBackend::kCuda;
  const uint8_t* landp = land_.empty() ? nullptr : land_.data();
  if (allow_cuda &&
      try_bake_hypso_thrust(heights_.data(), landp, cols_, rows_, step_x,
                            step_y, w, h, rgba->data())) {
    if (out_w) {
      *out_w = w;
    }
    if (out_h) {
      *out_h = h;
    }
    if (!skip_cache && !source_path_.empty() && !rgba->empty()) {
      dem_hypso_cache_put(source_path_.c_str(), max_edge, *rgba, w, h);
    }
    note_dem_phase_hypso(
        static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
        /*cache_hit=*/false);
    return true;
  }
  if (require_cuda) {
    return false;
  }
  uint8_t* pixels = rgba->data();
  auto fill_row = [&](int row) {
    const int src_row = (std::min)(rows_ - 1, row * step_y);
    for (int col = 0; col < w; ++col) {
      const int src_col = (std::min)(cols_ - 1, col * step_x);
      float r = 0.05f;
      float g = 0.08f;
      float b = 0.14f;
      bool land = true;
      if (!land_.empty()) {
        land = land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
      }
      if (land) {
        float m = meters_at(src_col, src_row);
        if (m <= 1.f) {
          m = 80.f;
        }
        const int c1 = (std::min)(cols_ - 1, src_col + step_x);
        const int r1 = (std::min)(rows_ - 1, src_row + step_y);
        const float m0 = meters_at(src_col, src_row);
        const float dh = std::fabs(meters_at(c1, src_row) - m0);
        const float dv = std::fabs(meters_at(src_col, r1) - m0);
        const float slope =
            detail::dem_clampf(std::sqrt(dh * dh + dv * dv) / 900.f, 0.f, 1.f);
        terrain_material_rgb(m, slope, &r, &g, &b);
      }
      const size_t i =
          (static_cast<size_t>(row) * static_cast<size_t>(w) +
           static_cast<size_t>(col)) *
          4u;
      pixels[i + 0] = detail::bake_pack_u8(r);
      pixels[i + 1] = detail::bake_pack_u8(g);
      pixels[i + 2] = detail::bake_pack_u8(b);
      pixels[i + 3] = 255;
    }
  };
  detail::for_each_bake_row(w, h, fill_row);
  if (!land_.empty() && w > 2 && h > 2) {
    auto land_at = [&](int c, int r) -> bool {
      const int src_col = (std::min)(cols_ - 1, c * step_x);
      const int src_row = (std::min)(rows_ - 1, r * step_y);
      return land_[static_cast<size_t>(index_at(src_col, src_row))] != 0;
    };
    std::vector<uint8_t> alt(rgba->size());
    uint8_t* a = rgba->data();
    uint8_t* b = alt.data();
    for (int pass = 0; pass < 3; ++pass) {
      const uint8_t* srcp = a;
      uint8_t* dstp = b;
      auto dilate_row = [&](int row) {
        for (int col = 0; col < w; ++col) {
          const size_t dst =
              (static_cast<size_t>(row) * static_cast<size_t>(w) +
               static_cast<size_t>(col)) *
              4u;
          dstp[dst + 0] = srcp[dst + 0];
          dstp[dst + 1] = srcp[dst + 1];
          dstp[dst + 2] = srcp[dst + 2];
          dstp[dst + 3] = srcp[dst + 3];
          if (land_at(col, row)) {
            continue;
          }
          const int nbs[4][2] = {{col - 1, row},
                                 {col + 1, row},
                                 {col, row - 1},
                                 {col, row + 1}};
          for (const auto& nb : nbs) {
            const int nc = nb[0];
            const int nr = nb[1];
            if (nc < 0 || nr < 0 || nc >= w || nr >= h) {
              continue;
            }
            const size_t src =
                (static_cast<size_t>(nr) * static_cast<size_t>(w) +
                 static_cast<size_t>(nc)) *
                4u;
            if (srcp[src + 0] < 40 && srcp[src + 1] < 55 &&
                srcp[src + 2] < 80 && !land_at(nc, nr)) {
              continue;
            }
            dstp[dst + 0] = srcp[src + 0];
            dstp[dst + 1] = srcp[src + 1];
            dstp[dst + 2] = srcp[src + 2];
            dstp[dst + 3] = 255;
            break;
          }
        }
      };
      detail::for_each_bake_row(w, h, dilate_row);
      uint8_t* tmp = a;
      a = b;
      b = tmp;
    }
    if (a != rgba->data()) {
      rgba->swap(alt);
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
  if (!skip_cache && !source_path_.empty() && !rgba->empty()) {
    dem_hypso_cache_put(source_path_.c_str(), max_edge, *rgba, w, h);
  }
  note_dem_phase_hypso(
      static_cast<int64_t>(hypso_timer.elapsed_milliseconds() + 0.5),
      /*cache_hit=*/false);
  return true;
}

}  // namespace vista

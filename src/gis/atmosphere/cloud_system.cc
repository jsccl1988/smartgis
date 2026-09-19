// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/cloud_system.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "gis/atmosphere/field_channel.h"

namespace gis {
namespace atmosphere {
namespace {

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

// Hash-based value noise in [0,1] for optional cover advection.
float hash01(int x, int y) {
  uint32_t n = static_cast<uint32_t>(x) * 374761393u +
               static_cast<uint32_t>(y) * 668265263u;
  n = (n ^ (n >> 13u)) * 1274126177u;
  return static_cast<float>((n ^ (n >> 16u)) & 0x00FFFFFFu) /
         static_cast<float>(0x01000000u);
}

float sample_wind_at_cell(const FieldStore& store, FieldChannel channel,
                          const FieldGrid& grid, int col, int row,
                          double time_sec) {
  if (grid.empty()) {
    return 0.0f;
  }
  const double lon_span = grid.max_lon - grid.min_lon;
  const double lat_span = grid.max_lat - grid.min_lat;
  const double u =
      (grid.cols <= 1) ? 0.0
                       : static_cast<double>(col) /
                             static_cast<double>(grid.cols - 1);
  const double v =
      (grid.rows <= 1) ? 0.0
                       : static_cast<double>(row) /
                             static_cast<double>(grid.rows - 1);
  const double lon = grid.min_lon + u * lon_span;
  const double lat = grid.min_lat + v * lat_span;
  return store.sample(channel, lon, lat, time_sec);
}

}  // namespace

CloudSystem::CloudSystem() = default;
CloudSystem::~CloudSystem() = default;

CloudSample CloudSystem::sample_at(const FieldStore& store, double lon,
                                   double lat, double time_sec) const {
  CloudSample out;
  out.cover = store.sample(FieldChannel::kCloudCover, lon, lat, time_sec);
  out.base_m = store.sample(FieldChannel::kCloudBase, lon, lat, time_sec);
  out.top_m = store.sample(FieldChannel::kCloudTop, lon, lat, time_sec);
  out.wind_u = store.sample(FieldChannel::kWindU, lon, lat, time_sec);
  out.wind_v = store.sample(FieldChannel::kWindV, lon, lat, time_sec);
  if (out.top_m < out.base_m) {
    std::swap(out.base_m, out.top_m);
  }
  out.cover = clampf(out.cover, 0.0f, 1.0f);
  return out;
}

float CloudSystem::density_from_cover(float cover, float height_m, float base_m,
                                      float top_m, float noise01) {
  const float c = clampf(cover, 0.0f, 1.0f);
  const float n = clampf(noise01, 0.0f, 1.0f);
  if (c <= 0.0f) {
    return 0.0f;
  }
  float lo = base_m;
  float hi = top_m;
  if (hi < lo) {
    std::swap(lo, hi);
  }
  const float thickness = hi - lo;
  if (thickness <= 1.0e-3f) {
    return 0.0f;
  }
  if (height_m < lo || height_m > hi) {
    return 0.0f;
  }
  // Soft vertical profile peaking mid-slab.
  const float t = (height_m - lo) / thickness;
  const float falloff = 4.0f * t * (1.0f - t);
  return c * falloff * n;
}

int CloudSystem::raymarch_steps_for_quality(int quality) {
  // quality 0..3 → 8 / 16 / 32 / 64 (clamp outside).
  const int q = std::max(0, std::min(3, quality));
  return 8 << q;
}

bool CloudSystem::advect_cover(FieldStore* store,
                               const CloudAdvectionParams& params,
                               double dt_sec, double time_sec) const {
  if (!store || !params.enabled || !(dt_sec > 0.0)) {
    return false;
  }

  const FieldLayer* cover_layer = nullptr;
  for (std::size_t i = 0; i < store->layer_count(); ++i) {
    const FieldLayer* layer = store->layer_at(i);
    if (layer && layer->channel == FieldChannel::kCloudCover &&
        !layer->grid.empty() &&
        layer->values.size() == layer->grid.cell_count()) {
      if (!cover_layer || layer->priority >= cover_layer->priority) {
        cover_layer = layer;
      }
    }
  }
  if (!cover_layer) {
    return false;
  }

  FieldLayer next = *cover_layer;
  const FieldGrid& grid = next.grid;
  const float dt = static_cast<float>(dt_sec);
  for (int row = 0; row < grid.rows; ++row) {
    for (int col = 0; col < grid.cols; ++col) {
      const std::size_t idx =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
          static_cast<std::size_t>(col);
      if (!next.valid_mask.empty() && next.valid_mask[idx] == 0) {
        continue;
      }
      const float wu =
          sample_wind_at_cell(*store, FieldChannel::kWindU, grid, col, row,
                              time_sec);
      const float wv =
          sample_wind_at_cell(*store, FieldChannel::kWindV, grid, col, row,
                              time_sec);
      // Integer lattice shift from wind; fractional part feeds noise phase.
      const float shift_x = wu * params.speed_scale * dt;
      const float shift_y = wv * params.speed_scale * dt;
      const int nx = col + static_cast<int>(std::floor(shift_x));
      const int ny = row + static_cast<int>(std::floor(shift_y));
      const float noise =
          hash01(nx, ny) * 2.0f - 1.0f;  // [-1,1]
      const float phase =
          hash01(col, row) +
          (shift_x + shift_y) * params.noise_scale;
      const float wobble =
          std::sin(phase * 6.28318530718f) * params.strength * dt;
      float cover = next.values[idx];
      cover = clampf(cover + noise * params.strength * dt + wobble, 0.0f,
                     1.0f);
      next.values[idx] = cover;
    }
  }
  next.time_sec = time_sec;
  store->set_layer(next);
  return true;
}

}  // namespace atmosphere
}  // namespace gis

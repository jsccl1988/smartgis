// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/procedural.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace gis {
namespace atmosphere {
namespace {

float hash01(unsigned x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return static_cast<float>(x & 0xffffu) / 65535.f;
}

float value_noise2(double lon, double lat, unsigned seed) {
  const int ix = static_cast<int>(std::floor(lon * 4.0));
  const int iy = static_cast<int>(std::floor(lat * 4.0));
  const float fx = static_cast<float>(lon * 4.0 - std::floor(lon * 4.0));
  const float fy = static_cast<float>(lat * 4.0 - std::floor(lat * 4.0));
  const float sx = fx * fx * (3.f - 2.f * fx);
  const float sy = fy * fy * (3.f - 2.f * fy);
  auto at = [&](int x, int y) {
    return hash01(seed + static_cast<unsigned>(x * 374761393 + y * 668265263));
  };
  const float a = at(ix, iy);
  const float b = at(ix + 1, iy);
  const float c = at(ix, iy + 1);
  const float d = at(ix + 1, iy + 1);
  const float ab = a * (1.f - sx) + b * sx;
  const float cd = c * (1.f - sx) + d * sx;
  return ab * (1.f - sy) + cd * sy;
}

void node_lonlat(const FieldGrid& grid, int col, int row, double* lon,
                 double* lat) {
  if (grid.cols <= 1) {
    *lon = grid.min_lon;
  } else {
    *lon = grid.min_lon +
           (grid.max_lon - grid.min_lon) *
               (static_cast<double>(col) / static_cast<double>(grid.cols - 1));
  }
  if (grid.rows <= 1) {
    *lat = grid.min_lat;
  } else {
    *lat = grid.min_lat +
           (grid.max_lat - grid.min_lat) *
               (static_cast<double>(row) / static_cast<double>(grid.rows - 1));
  }
}

FieldLayer make_layer(FieldChannel channel, const FieldGrid& grid, int priority,
                      std::vector<float> values) {
  FieldLayer layer;
  layer.channel = channel;
  layer.kind = FieldSourceKind::kProcedural;
  layer.priority = priority;
  layer.grid = grid;
  layer.values = std::move(values);
  layer.time_sec = std::numeric_limits<double>::quiet_NaN();
  return layer;
}

}  // namespace

void WindNoise::apply(FieldStore* store, const FieldGrid& grid,
                      int priority) const {
  if (!store || grid.empty()) {
    return;
  }
  std::vector<float> u(grid.cell_count());
  std::vector<float> v(grid.cell_count());
  for (int row = 0; row < grid.rows; ++row) {
    for (int col = 0; col < grid.cols; ++col) {
      double lon = 0.0;
      double lat = 0.0;
      node_lonlat(grid, col, row, &lon, &lat);
      const std::size_t idx =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
          static_cast<std::size_t>(col);
      const float n0 = value_noise2(lon, lat, opts.seed);
      const float n1 = value_noise2(lon + 17.3, lat - 9.1, opts.seed + 91u);
      u[idx] = (n0 * 2.f - 1.f) * opts.wind_scale;
      v[idx] = (n1 * 2.f - 1.f) * opts.wind_scale;
    }
  }
  store->set_layer(make_layer(FieldChannel::kWindU, grid, priority, std::move(u)));
  store->set_layer(make_layer(FieldChannel::kWindV, grid, priority, std::move(v)));
}

void WaveFromWind::apply(FieldStore* store, const FieldGrid& grid,
                         int priority) const {
  if (!store || grid.empty()) {
    return;
  }
  std::vector<float> hs(grid.cell_count());
  std::vector<float> dir(grid.cell_count());
  for (int row = 0; row < grid.rows; ++row) {
    for (int col = 0; col < grid.cols; ++col) {
      double lon = 0.0;
      double lat = 0.0;
      node_lonlat(grid, col, row, &lon, &lat);
      const std::size_t idx =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
          static_cast<std::size_t>(col);
      const float wu =
          store->sample(FieldChannel::kWindU, lon, lat, /*time_sec=*/0.0);
      const float wv =
          store->sample(FieldChannel::kWindV, lon, lat, /*time_sec=*/0.0);
      const float speed = std::sqrt(wu * wu + wv * wv);
      hs[idx] = speed * opts.wave_hs_scale;
      dir[idx] = std::atan2(wv, wu);
    }
  }
  store->set_layer(
      make_layer(FieldChannel::kWaveHs, grid, priority, std::move(hs)));
  store->set_layer(
      make_layer(FieldChannel::kWaveDir, grid, priority, std::move(dir)));
}

void CloudNoise::apply(FieldStore* store, const FieldGrid& grid,
                       int priority) const {
  if (!store || grid.empty()) {
    return;
  }
  std::vector<float> cover(grid.cell_count());
  std::vector<float> base(grid.cell_count());
  std::vector<float> top(grid.cell_count());
  for (int row = 0; row < grid.rows; ++row) {
    for (int col = 0; col < grid.cols; ++col) {
      double lon = 0.0;
      double lat = 0.0;
      node_lonlat(grid, col, row, &lon, &lat);
      const std::size_t idx =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
          static_cast<std::size_t>(col);
      float c = value_noise2(lon * 0.6, lat * 0.6, opts.seed + 7u);
      c = (std::max)(0.f, (std::min)(1.f, c * opts.cloud_cover_scale));
      cover[idx] = c;
      base[idx] = opts.cloud_base_m + 200.f * value_noise2(lon, lat, opts.seed + 3u);
      top[idx] = base[idx] + opts.cloud_thickness_m * (0.5f + 0.5f * c);
    }
  }
  store->set_layer(
      make_layer(FieldChannel::kCloudCover, grid, priority, std::move(cover)));
  store->set_layer(
      make_layer(FieldChannel::kCloudBase, grid, priority, std::move(base)));
  store->set_layer(
      make_layer(FieldChannel::kCloudTop, grid, priority, std::move(top)));
}

void SeaMaskFromLand::apply(FieldStore* store, const FieldGrid& grid,
                            const std::vector<LonLatRing>& land_rings,
                            int priority) const {
  if (!store || grid.empty()) {
    return;
  }
  std::vector<float> sea(grid.cell_count());
  for (int row = 0; row < grid.rows; ++row) {
    for (int col = 0; col < grid.cols; ++col) {
      double lon = 0.0;
      double lat = 0.0;
      node_lonlat(grid, col, row, &lon, &lat);
      const std::size_t idx =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(grid.cols) +
          static_cast<std::size_t>(col);
      const bool land = any_ring_contains(lon, lat, land_rings);
      sea[idx] = land ? 0.f : 1.f;
    }
  }
  store->set_layer(
      make_layer(FieldChannel::kSeaMask, grid, priority, std::move(sea)));
}

void procedural_step(FieldStore* /*store*/, double /*dt_sec*/) {
  // Reserved for shallow-water / cloud advection; intentionally empty in v1.
}

}  // namespace atmosphere
}  // namespace gis

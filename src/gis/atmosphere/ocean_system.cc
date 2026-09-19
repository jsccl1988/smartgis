// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/ocean_system.h"

#include <algorithm>
#include <cmath>

namespace gis {
namespace atmosphere {
namespace {

float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(hi, v));
}

int fft_size_for_quality(int quality) {
  // quality 0 → Gerstner; 1 → 32; 2 → 64; 3+ → 128.
  if (quality <= 0) {
    return 0;
  }
  if (quality == 1) {
    return 32;
  }
  if (quality == 2) {
    return 64;
  }
  return 128;
}

void cell_center_lon_lat(const FieldGrid& extent, int cols, int rows, int c,
                         int r, double* lon, double* lat) {
  const double dlon =
      (extent.max_lon - extent.min_lon) / static_cast<double>(cols);
  const double dlat =
      (extent.max_lat - extent.min_lat) / static_cast<double>(rows);
  *lon = extent.min_lon + (static_cast<double>(c) + 0.5) * dlon;
  *lat = extent.min_lat + (static_cast<double>(r) + 0.5) * dlat;
}

bool fill_channel_grid(const FieldStore& store, FieldChannel channel,
                       const FieldGrid& extent, int cols, int rows,
                       double time_sec, float* out, std::size_t out_count) {
  if (!out || cols < 1 || rows < 1) {
    return false;
  }
  if (extent.empty() || extent.max_lon <= extent.min_lon ||
      extent.max_lat <= extent.min_lat) {
    return false;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (out_count != need) {
    return false;
  }
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      double lon = 0.0;
      double lat = 0.0;
      cell_center_lon_lat(extent, cols, rows, c, r, &lon, &lat);
      out[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) +
          static_cast<std::size_t>(c)] =
          store.sample(channel, lon, lat, time_sec);
    }
  }
  return true;
}

}  // namespace

OceanSpectrumParams make_ocean_spectrum(float hs, float wave_dir_rad,
                                        float wind_u, float wind_v,
                                        int quality) {
  OceanSpectrumParams p;
  p.significant_wave_height = std::max(0.0f, hs);
  p.mean_direction_rad = wave_dir_rad;
  p.wind_speed = std::sqrt(wind_u * wind_u + wind_v * wind_v);
  p.wind_direction_rad =
      (p.wind_speed > 1.0e-6f) ? std::atan2(wind_v, wind_u) : wave_dir_rad;
  // Prefer wind direction when Hs is calm but wind is present (Phillips drive).
  if (p.significant_wave_height < 1.0e-4f && p.wind_speed > 0.5f) {
    p.mean_direction_rad = p.wind_direction_rad;
    // Beaufort-ish Hs estimate (meters) from wind speed (m/s).
    p.significant_wave_height =
        clampf(0.024f * p.wind_speed * p.wind_speed, 0.05f, 8.0f);
  }
  p.fft_size = fft_size_for_quality(quality);
  p.use_gerstner_fallback = (p.fft_size < 16);
  // Quality 0 → Gerstner; otherwise JONSWAP. Explicit Phillips is a draw-path
  // override (OceanDrawParams::use_jonswap = false), not a quality tier.
  p.use_jonswap = (quality >= 1);
  p.chop = 1.0f;
  p.jonswap_gamma = 3.3f;
  return p;
}

OceanSystem::OceanSystem() = default;
OceanSystem::~OceanSystem() = default;

void OceanSystem::set_quality(int quality) {
  quality_ = quality;
}

OceanTileParams OceanSystem::sample_tile(const FieldStore& store,
                                         const FieldGrid& extent,
                                         double time_sec) const {
  OceanTileParams tile;
  tile.extent = extent;

  double lon = 0.5 * (extent.min_lon + extent.max_lon);
  double lat = 0.5 * (extent.min_lat + extent.max_lat);
  if (!extent.empty() && extent.max_lon > extent.min_lon &&
      extent.max_lat > extent.min_lat) {
    tile.hs = store.sample(FieldChannel::kWaveHs, lon, lat, time_sec);
    tile.wave_dir_rad =
        store.sample(FieldChannel::kWaveDir, lon, lat, time_sec);
    tile.wind_u = store.sample(FieldChannel::kWindU, lon, lat, time_sec);
    tile.wind_v = store.sample(FieldChannel::kWindV, lon, lat, time_sec);
    tile.sea_mask = store.sample(FieldChannel::kSeaMask, lon, lat, time_sec);
  }

  tile.spectrum = make_ocean_spectrum(tile.hs, tile.wave_dir_rad, tile.wind_u,
                                      tile.wind_v, quality_);
  return tile;
}

bool OceanSystem::fill_sea_mask_grid(const FieldStore& store,
                                     const FieldGrid& extent, int cols,
                                     int rows, double time_sec,
                                     float* out_mask,
                                     std::size_t out_count) const {
  return fill_channel_grid(store, FieldChannel::kSeaMask, extent, cols, rows,
                           time_sec, out_mask, out_count);
}

bool OceanSystem::fill_hs_grid(const FieldStore& store, const FieldGrid& extent,
                               int cols, int rows, double time_sec,
                               float* out_hs, std::size_t out_count) const {
  return fill_channel_grid(store, FieldChannel::kWaveHs, extent, cols, rows,
                           time_sec, out_hs, out_count);
}

}  // namespace atmosphere
}  // namespace gis

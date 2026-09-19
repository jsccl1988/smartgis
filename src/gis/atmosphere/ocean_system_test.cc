// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/ocean_system.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldGrid;
  using gis::atmosphere::FieldLayer;
  using gis::atmosphere::FieldSourceKind;
  using gis::atmosphere::FieldStore;
  using gis::atmosphere::OceanSystem;
  using gis::atmosphere::make_ocean_spectrum;

  // Pure spectrum: Hs + dir → FFT path.
  {
    const auto s = make_ocean_spectrum(2.0f, 0.5f, 0.0f, 0.0f, 2);
    expect(s.significant_wave_height == 2.0f, "hs passthrough");
    expect(std::fabs(s.mean_direction_rad - 0.5f) < 1.0e-5f, "dir");
    expect(s.fft_size == 64, "quality 2 → 64");
    expect(!s.use_gerstner_fallback, "fft path");
    expect(s.use_jonswap, "jonswap default");
  }

  // Quality 0 forces Gerstner fallback.
  {
    const auto s = make_ocean_spectrum(1.0f, 0.0f, 3.0f, 4.0f, 0);
    expect(s.fft_size == 0, "quality 0 fft_size");
    expect(s.use_gerstner_fallback, "gerstner fallback");
    expect(std::fabs(s.wind_speed - 5.0f) < 1.0e-4f, "wind speed");
  }

  // Calm Hs + wind → estimated Hs from Beaufort-ish curve.
  {
    const auto s = make_ocean_spectrum(0.0f, 0.0f, 10.0f, 0.0f, 1);
    expect(s.significant_wave_height > 0.05f, "wind-driven hs");
    expect(s.fft_size == 32, "quality 1 → 32");
  }

  OceanSystem ocean;
  ocean.set_quality(2);
  expect(ocean.quality() == 2, "quality set");

  FieldStore store;
  FieldGrid extent;
  extent.min_lon = 120.0;
  extent.min_lat = 30.0;
  extent.max_lon = 122.0;
  extent.max_lat = 32.0;
  extent.cols = 4;
  extent.rows = 4;

  // Upload layers (FieldStore::sample may still be stubbed by Field lane).
  {
    FieldLayer hs;
    hs.channel = FieldChannel::kWaveHs;
    hs.kind = FieldSourceKind::kProcedural;
    hs.priority = 1;
    hs.grid = extent;
    hs.values.assign(extent.cell_count(), 1.5f);
    store.set_layer(hs);

    FieldLayer mask;
    mask.channel = FieldChannel::kSeaMask;
    mask.kind = FieldSourceKind::kProcedural;
    mask.priority = 1;
    mask.grid = extent;
    mask.values.assign(extent.cell_count(), 1.0f);
    store.set_layer(mask);
  }

  const auto tile = ocean.sample_tile(store, extent, 0.0);
  expect(tile.extent.cols == 4, "tile extent");
  expect(std::fabs(tile.hs - 1.5f) < 1.0e-4f, "tile hs from FieldStore");
  expect(std::fabs(tile.sea_mask - 1.0f) < 1.0e-4f, "tile sea mask");
  expect(tile.spectrum.fft_size == 64, "tile spectrum fft");
  expect(std::fabs(tile.spectrum.significant_wave_height - 1.5f) < 1.0e-4f,
         "spectrum hs");

  std::vector<float> masks(16, 0.0f);
  expect(ocean.fill_sea_mask_grid(store, extent, 4, 4, 0.0, masks.data(),
                                  masks.size()),
         "fill sea mask grid");
  expect(std::fabs(masks[0] - 1.0f) < 1.0e-4f, "mask sample");

  std::vector<float> hs_grid(16, 0.0f);
  expect(ocean.fill_hs_grid(store, extent, 4, 4, 0.0, hs_grid.data(),
                            hs_grid.size()),
         "fill hs grid");
  expect(std::fabs(hs_grid[0] - 1.5f) < 1.0e-4f, "hs sample");

  expect(!ocean.fill_sea_mask_grid(store, extent, 4, 4, 0.0, nullptr, 0),
         "reject null out");

  if (g_fails != 0) {
    std::fprintf(stderr, "ocean_system_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "ocean_system_test: ok\n");
  return 0;
}

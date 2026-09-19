// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/procedural.h"

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

void expect_near(float got, float want, float eps, const char* msg) {
  if (!(std::abs(got - want) <= eps)) {
    std::fprintf(stderr, "FAIL: %s (got=%g want=%g)\n", msg, got, want);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using gis::atmosphere::CloudNoise;
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldGrid;
  using gis::atmosphere::FieldStore;
  using gis::atmosphere::SeaMaskFromLand;
  using gis::atmosphere::WaveFromWind;
  using gis::atmosphere::WindNoise;

  FieldGrid grid;
  grid.min_lon = 100.0;
  grid.min_lat = 20.0;
  grid.max_lon = 120.0;
  grid.max_lat = 40.0;
  grid.cols = 8;
  grid.rows = 8;

  FieldStore store;
  WindNoise wind;
  wind.opts.seed = 42;
  wind.apply(&store, grid, /*priority=*/0);
  expect(store.layer_count() >= 2, "wind layers present");

  const float u = store.sample(FieldChannel::kWindU, 110.0, 30.0, 0.0);
  const float v = store.sample(FieldChannel::kWindV, 110.0, 30.0, 0.0);
  expect(std::isfinite(u) && std::isfinite(v), "wind sample finite");
  expect(std::abs(u) > 1e-6f || std::abs(v) > 1e-6f, "wind non-zero somewhere");

  WaveFromWind wave;
  wave.apply(&store, grid, /*priority=*/0);
  const float hs = store.sample(FieldChannel::kWaveHs, 110.0, 30.0, 0.0);
  expect(std::isfinite(hs) && hs >= 0.f, "wave hs non-negative");

  CloudNoise cloud;
  cloud.apply(&store, grid, /*priority=*/0);
  const float cover =
      store.sample(FieldChannel::kCloudCover, 110.0, 30.0, 0.0);
  expect(std::isfinite(cover), "cloud cover finite");
  expect(cover >= 0.f && cover <= 1.f, "cloud cover in [0,1]");

  gis::LonLatRing land;
  land.x = {105.0, 115.0, 115.0, 105.0};
  land.y = {25.0, 25.0, 35.0, 35.0};
  land.prepare_bbox();
  std::vector<gis::LonLatRing> rings = {land};
  SeaMaskFromLand sea;
  sea.apply(&store, grid, rings, /*priority=*/0);

  const float sea_in =
      store.sample(FieldChannel::kSeaMask, 110.0, 30.0, 0.0);
  const float sea_out =
      store.sample(FieldChannel::kSeaMask, 101.0, 21.0, 0.0);
  expect_near(sea_in, 0.f, 1e-4f, "inside land => sea=0");
  expect_near(sea_out, 1.f, 1e-4f, "outside land => sea=1");
  expect(gis::any_ring_contains(110.0, 30.0, rings), "land PIP dual");
  expect(!gis::any_ring_contains(101.0, 21.0, rings), "ocean PIP dual");

  gis::atmosphere::procedural_step(&store, 0.1);
  expect(true, "procedural_step no-op ok");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d procedural check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "procedural_test OK\n");
  return 0;
}

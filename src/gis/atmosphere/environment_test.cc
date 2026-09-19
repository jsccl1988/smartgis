// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/environment.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include "gis/atmosphere/field_channel.h"
#include "gis/atmosphere/field_store.h"

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
  using gis::atmosphere::Environment;
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldGrid;

  Environment env;
  expect(!env.ocean_enabled(), "default ocean off");
  expect(!env.cloud_enabled(), "default cloud off");

  FieldGrid grid;
  grid.min_lon = 73.0;
  grid.min_lat = 18.0;
  grid.max_lon = 135.0;
  grid.max_lat = 54.0;
  grid.cols = 16;
  grid.rows = 16;

  env.enable_demo(grid, nullptr);
  expect(env.ocean_enabled(), "demo ocean on");
  expect(env.cloud_enabled(), "demo cloud on");
  expect(env.field_store().layer_count() >= 4, "procedural layers seeded");

  const float cover =
      env.field_store().sample(FieldChannel::kCloudCover, 100.0, 30.0, 0.0);
  expect(std::isfinite(cover), "cover finite");
  const float hs =
      env.field_store().sample(FieldChannel::kWaveHs, 100.0, 30.0, 0.0);
  expect(std::isfinite(hs) && hs >= 0.f, "hs finite");
  const float sea =
      env.field_store().sample(FieldChannel::kSeaMask, 100.0, 30.0, 0.0);
  // No land rings → all sea.
  expect(sea > 0.5f, "empty rings => sea");

  env.set_ocean_enabled(false);
  expect(!env.ocean_enabled(), "setter clears ocean");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d environment_test fail(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "environment_test OK\n");
  return 0;
}

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

void expect_near(float got, float want, float eps, const char* msg) {
  if (!(std::abs(got - want) <= eps)) {
    std::fprintf(stderr, "FAIL: %s (got=%g want=%g)\n", msg, got, want);
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

  // Session scrub + timed range clamp (upload_slice; no GDAL required).
  {
    using gis::atmosphere::FieldSourceKind;
    const auto& grid_ref = grid;
    float v0[] = {1.f};
    float v1[] = {3.f};
    gis::atmosphere::FieldGrid one = grid_ref;
    one.cols = 1;
    one.rows = 1;
    expect(env.field_store().upload_slice(FieldChannel::kCloudCover,
                                          FieldSourceKind::kExternal, 20, one,
                                          v0, 1, nullptr, 0, 100.0),
           "env upload t0");
    expect(env.field_store().upload_slice(FieldChannel::kCloudCover,
                                          FieldSourceKind::kExternal, 20, one,
                                          v1, 1, nullptr, 0, 200.0),
           "env upload t1");

    double t_min = 0.0;
    double t_max = 0.0;
    expect(env.timed_field_range(FieldChannel::kCloudCover, &t_min, &t_max),
           "env timed range");
    expect(t_min == 100.0 && t_max == 200.0, "env range 100..200");

    env.scrub_time_sec(150.0);
    expect(env.time_sec() == 150.0, "scrub absolute");
    env.advance_time_sec(10.0);
    expect(env.time_sec() == 160.0, "advance");
    env.scrub_time_sec(50.0);
    expect(env.clamp_time_to_field(FieldChannel::kCloudCover), "clamp");
    expect(env.time_sec() == 100.0, "clamped to min");
    expect_near(env.field_store().sample(FieldChannel::kCloudCover, 100.0, 30.0,
                                         env.time_sec()),
                1.f, 1e-4f, "sample at scrubbed time");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d environment_test fail(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "environment_test OK\n");
  return 0;
}

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/field_store.h"

#include <cmath>
#include <cstdio>
#include <limits>
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

gis::atmosphere::FieldGrid make_grid(int cols, int rows) {
  gis::atmosphere::FieldGrid g;
  g.min_lon = 0.0;
  g.min_lat = 0.0;
  g.max_lon = 10.0;
  g.max_lat = 10.0;
  g.cols = cols;
  g.rows = rows;
  return g;
}

}  // namespace

int main() {
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldLayer;
  using gis::atmosphere::FieldSourceKind;
  using gis::atmosphere::FieldStore;

  // Priority + valid_mask mix: high priority wins where valid; else fallthrough.
  {
    FieldStore store;
    const auto grid = make_grid(2, 2);
    FieldLayer low;
    low.channel = FieldChannel::kWaveHs;
    low.kind = FieldSourceKind::kProcedural;
    low.priority = 1;
    low.grid = grid;
    low.values = {1.f, 1.f, 1.f, 1.f};
    low.time_sec = std::numeric_limits<double>::quiet_NaN();
    store.set_layer(low);

    FieldLayer high;
    high.channel = FieldChannel::kWaveHs;
    high.kind = FieldSourceKind::kExternal;
    high.priority = 10;
    high.grid = grid;
    high.values = {9.f, 9.f, 9.f, 9.f};
    high.valid_mask = {1, 0, 1, 0};  // only west column valid
    high.time_sec = std::numeric_limits<double>::quiet_NaN();
    store.set_layer(high);

    expect_near(store.sample(FieldChannel::kWaveHs, 0.0, 0.0, 0.0), 9.f, 1e-4f,
                "high priority at SW");
    expect_near(store.sample(FieldChannel::kWaveHs, 10.0, 0.0, 0.0), 1.f, 1e-4f,
                "fallthrough where high mask invalid");
  }

  // Spatial clamp outside envelope.
  {
    FieldStore store;
    const auto grid = make_grid(2, 1);
    FieldLayer layer;
    layer.channel = FieldChannel::kWindU;
    layer.kind = FieldSourceKind::kProcedural;
    layer.priority = 0;
    layer.grid = grid;
    layer.values = {3.f, 7.f};
    layer.time_sec = std::numeric_limits<double>::quiet_NaN();
    store.set_layer(layer);
    expect_near(store.sample(FieldChannel::kWindU, -50.0, 5.0, 0.0), 3.f, 1e-4f,
                "clamp west");
    expect_near(store.sample(FieldChannel::kWindU, 99.0, 5.0, 0.0), 7.f, 1e-4f,
                "clamp east");
  }

  // Temporal linear lerp + clamp via upload_slice time keys.
  {
    FieldStore store;
    const auto grid = make_grid(1, 1);
    const float v0[] = {0.f};
    const float v1[] = {10.f};
    expect(store.upload_slice(FieldChannel::kCloudCover,
                              FieldSourceKind::kProcedural, 0, grid, v0, 1,
                              nullptr, 0, 0.0),
           "upload t0");
    expect(store.upload_slice(FieldChannel::kCloudCover,
                              FieldSourceKind::kProcedural, 0, grid, v1, 1,
                              nullptr, 0, 10.0),
           "upload t1");
    expect(store.layer_count() == 2, "two time slices");
    expect_near(store.sample(FieldChannel::kCloudCover, 5.0, 5.0, 5.0), 5.f,
                1e-4f, "mid lerp");
    expect_near(store.sample(FieldChannel::kCloudCover, 5.0, 5.0, -1.0), 0.f,
                1e-4f, "time clamp low");
    expect_near(store.sample(FieldChannel::kCloudCover, 5.0, 5.0, 20.0), 10.f,
                1e-4f, "time clamp high");

    double t_min = -1.0;
    double t_max = -1.0;
    expect(store.timed_slice_range(FieldChannel::kCloudCover, &t_min, &t_max),
           "timed range exists");
    expect_near(static_cast<float>(t_min), 0.f, 1e-6f, "range min");
    expect_near(static_cast<float>(t_max), 10.f, 1e-6f, "range max");
  }

  // Temporal lerp + valid_mask: invalid bracket falls back to the other side.
  {
    FieldStore store;
    const auto grid = make_grid(1, 1);
    const float v0[] = {2.f};
    const float v1[] = {8.f};
    const uint8_t mask_bad[] = {0};
    const uint8_t mask_ok[] = {1};
    expect(store.upload_slice(FieldChannel::kWaveHs, FieldSourceKind::kExternal,
                              5, grid, v0, 1, mask_bad, 1, 0.0),
           "upload masked t0");
    expect(store.upload_slice(FieldChannel::kWaveHs, FieldSourceKind::kExternal,
                              5, grid, v1, 1, mask_ok, 1, 10.0),
           "upload valid t1");
    expect_near(store.sample(FieldChannel::kWaveHs, 5.0, 5.0, 5.0), 8.f, 1e-4f,
                "lerp uses valid bracket only");
  }

  // Empty channel → 0.
  {
    FieldStore store;
    expect_near(store.sample(FieldChannel::kSeaMask, 0.0, 0.0, 0.0), 0.f, 0.f,
                "empty returns 0");
    double t_min = 0.0;
    double t_max = 0.0;
    expect(!store.timed_slice_range(FieldChannel::kSeaMask, &t_min, &t_max),
           "empty has no timed range");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d field_store check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "field_store_test OK\n");
  return 0;
}

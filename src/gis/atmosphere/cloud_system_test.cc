// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/cloud_system.h"

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
  if (std::fabs(got - want) > eps) {
    std::fprintf(stderr, "FAIL: %s (got=%g want=%g)\n", msg, got, want);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using gis::atmosphere::CloudAdvectionParams;
  using gis::atmosphere::CloudSystem;
  using gis::atmosphere::FieldChannel;
  using gis::atmosphere::FieldGrid;
  using gis::atmosphere::FieldLayer;
  using gis::atmosphere::FieldSourceKind;
  using gis::atmosphere::FieldStore;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  // Pure density: zero cover → zero density.
  expect_near(CloudSystem::density_from_cover(0.0f, 2000.f, 1000.f, 3000.f, 1.f),
              0.0f, 1.0e-6f, "zero cover");

  // Outside slab → zero.
  expect_near(
      CloudSystem::density_from_cover(1.0f, 500.f, 1000.f, 3000.f, 1.f), 0.0f,
      1.0e-6f, "below base");
  expect_near(
      CloudSystem::density_from_cover(1.0f, 4000.f, 1000.f, 3000.f, 1.f), 0.0f,
      1.0e-6f, "above top");

  // Mid-slab + full cover + noise=1 → peak falloff = 1.
  expect_near(
      CloudSystem::density_from_cover(1.0f, 2000.f, 1000.f, 3000.f, 1.f), 1.0f,
      1.0e-4f, "mid slab peak");

  // Cover modulates density linearly at peak.
  expect_near(
      CloudSystem::density_from_cover(0.5f, 2000.f, 1000.f, 3000.f, 1.f), 0.5f,
      1.0e-4f, "cover half");

  // Noise modulates.
  expect_near(
      CloudSystem::density_from_cover(1.0f, 2000.f, 1000.f, 3000.f, 0.25f),
      0.25f, 1.0e-4f, "noise quarter");

  expect(CloudSystem::raymarch_steps_for_quality(0) == 8, "q0 steps");
  expect(CloudSystem::raymarch_steps_for_quality(1) == 16, "q1 steps");
  expect(CloudSystem::raymarch_steps_for_quality(2) == 32, "q2 steps");
  expect(CloudSystem::raymarch_steps_for_quality(3) == 64, "q3 steps");
  expect(CloudSystem::raymarch_steps_for_quality(99) == 64, "q clamp high");
  expect(CloudSystem::raymarch_steps_for_quality(-3) == 8, "q clamp low");

  // Advection writeback on an explicit cover layer (does not need sample()).
  {
    FieldStore store;
    FieldLayer cover;
    cover.channel = FieldChannel::kCloudCover;
    cover.kind = FieldSourceKind::kProcedural;
    cover.priority = 1;
    cover.grid.min_lon = 0.0;
    cover.grid.min_lat = 0.0;
    cover.grid.max_lon = 1.0;
    cover.grid.max_lat = 1.0;
    cover.grid.cols = 4;
    cover.grid.rows = 4;
    cover.values.assign(cover.grid.cell_count(), 0.5f);
    store.set_layer(cover);

    CloudSystem clouds;
    CloudAdvectionParams adv;
    adv.enabled = true;
    adv.strength = 0.2f;
    adv.speed_scale = 1.0f;
    adv.noise_scale = 0.1f;
    expect(clouds.advect_cover(&store, adv, 1.0, 0.0), "advect ok");
    const FieldLayer* after = store.layer_at(0);
    expect(after != nullptr && after->values.size() == 16, "cover layer");
    bool changed = false;
    for (float v : after->values) {
      expect(v >= 0.0f && v <= 1.0f, "cover clamped");
      if (std::fabs(v - 0.5f) > 1.0e-6f) {
        changed = true;
      }
    }
    expect(changed, "advection perturbs cover");

    adv.enabled = false;
    expect(!clouds.advect_cover(&store, adv, 1.0, 0.0), "disabled advect");
  }

  // sample_at wires FieldStore channels (stub sample returns 0 until Lane Field).
  {
    FieldStore store;
    CloudSystem clouds;
    const auto s = clouds.sample_at(store, 116.0, 40.0, 0.0);
    expect_near(s.cover, 0.0f, 1.0e-6f, "stub cover sample");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d cloud_system check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "cloud_system_test OK\n");
  return 0;
}

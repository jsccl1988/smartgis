// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/environment.h"

#include "gis/atmosphere/procedural.h"

namespace gis {
namespace atmosphere {

Environment::Environment() = default;
Environment::~Environment() = default;

void Environment::sync_systems_from_params() {
  ocean_system_.set_quality(params_.quality);
}

void Environment::seed_procedural_baseline(
    const FieldGrid& grid, const std::vector<LonLatRing>* land_rings) {
  if (grid.empty()) {
    return;
  }

  ProceduralOptions opts;
  opts.seed = 19;

  WindNoise wind;
  wind.opts = opts;
  wind.apply(&field_store_, grid, /*priority=*/0);

  WaveFromWind wave;
  wave.opts = opts;
  wave.apply(&field_store_, grid, /*priority=*/0);

  CloudNoise cloud;
  cloud.opts = opts;
  cloud.apply(&field_store_, grid, /*priority=*/0);

  SeaMaskFromLand sea;
  static const std::vector<LonLatRing> kEmpty;
  const std::vector<LonLatRing>& rings =
      land_rings ? *land_rings : kEmpty;
  sea.apply(&field_store_, grid, rings, /*priority=*/0);
}

void Environment::enable_demo(const FieldGrid& grid,
                              const std::vector<LonLatRing>* land_rings) {
  seed_procedural_baseline(grid, land_rings);
  params_.ocean_enabled = true;
  params_.cloud_enabled = true;
  sync_systems_from_params();
}

}  // namespace atmosphere
}  // namespace gis

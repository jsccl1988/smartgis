// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/atmosphere/environment.h"

#include <algorithm>

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

bool Environment::load_external_series(FieldChannel channel,
                                       const char* const* paths,
                                       const double* times, std::size_t count,
                                       const FieldIngestOptions& opts) {
  if (!ingest_gdal_field_series(&field_store_, channel, paths, times, count,
                                opts)) {
    return false;
  }
  if (count > 0 && times) {
    time_sec_ = times[0];
  }
  return true;
}

bool Environment::timed_field_range(FieldChannel channel, double* out_min,
                                    double* out_max) const {
  return field_store_.timed_slice_range(channel, out_min, out_max);
}

bool Environment::clamp_time_to_field(FieldChannel channel) {
  double t_min = 0.0;
  double t_max = 0.0;
  if (!timed_field_range(channel, &t_min, &t_max)) {
    return false;
  }
  time_sec_ = (std::max)(t_min, (std::min)(t_max, time_sec_));
  return true;
}

}  // namespace atmosphere
}  // namespace gis

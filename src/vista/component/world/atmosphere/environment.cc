// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/atmosphere/environment.h"

#include <algorithm>

#include "vista/component/world/atmosphere/field/procedural.h"

namespace vista {
namespace atmosphere {

Environment::Environment() = default;
Environment::~Environment() {
  contour_sheet_.clear();
}

void destroy_environment(Environment* p) {
  delete p;
}

EnvironmentPtr create_environment() {
  return EnvironmentPtr(new Environment());
}

void Environment::set_sky_enabled(bool on) {
  params_.sky_enabled = on;
}

void Environment::set_fog_enabled(bool on) {
  params_.fog_enabled = on;
}

bool Environment::sky_enabled() const {
  return params_.sky_enabled;
}

bool Environment::fog_enabled() const {
  return params_.fog_enabled;
}

void Environment::sync_systems_from_params() {
  ocean_system_.set_quality(params_.quality);
}

bool Environment::rebuild_contour_sheet(FieldChannel channel,
                                        const FieldGrid& grid,
                                        const float* dem_meters) {
  if (!params_.contour_enabled) {
    contour_sheet_.clear();
    return false;
  }
  ContourSheetOptions opts;
  opts.curves = params_.contour_curves;
  opts.surface = params_.contour_surface;
  opts.color_scale = params_.contour_color_scale;
  opts.dem_offset_m = params_.contour_dem_offset_m;
  opts.value_to_meters = params_.contour_value_to_meters;
  opts.dem_vert_exag = params_.contour_dem_vert_exag;
  opts.surface_alpha = params_.contour_surface_alpha;
  opts.scale_layout.margin = params_.contour_scale_margin;
  opts.scale_layout.bar_width = params_.contour_scale_bar_width;
  opts.scale_layout.bar_height = params_.contour_scale_bar_height;
  opts.scale_layout.tick_count = params_.contour_scale_tick_count;
  return contour_sheet_.rebuild_from_store(field_store_, channel, grid,
                                           time_sec_, dem_meters, opts);
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
  params_.sky_enabled = true;
  params_.fog_enabled = true;
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
}  // namespace vista

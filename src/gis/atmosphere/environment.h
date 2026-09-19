// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_ENVIRONMENT_H_
#define GIS_ATMOSPHERE_ENVIRONMENT_H_

#include <vector>

#include "gis/atmosphere/atmosphere_params.h"
#include "gis/atmosphere/cloud_system.h"
#include "gis/atmosphere/field_store.h"
#include "gis/atmosphere/ocean_system.h"
#include "gis/gis_export.h"
#include "gis/world/land_mask.h"

namespace gis {
namespace atmosphere {

// Session holder for atmosphere time, sun/quality params, shared FieldStore,
// and the Ocean/Cloud systems that drive GPU passes. Mounted optionally on
// MapScene / Scene3dController (not NodeKind). Ocean/cloud default off.
class GIS_EXPORT Environment {
 public:
  Environment();
  ~Environment();

  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  AtmosphereParams& params() { return params_; }
  const AtmosphereParams& params() const { return params_; }

  FieldStore& field_store() { return field_store_; }
  const FieldStore& field_store() const { return field_store_; }

  OceanSystem& ocean_system() { return ocean_system_; }
  const OceanSystem& ocean_system() const { return ocean_system_; }

  CloudSystem& cloud_system() { return cloud_system_; }
  const CloudSystem& cloud_system() const { return cloud_system_; }

  // Simulation / animation clock (seconds). Hosts advance this each frame.
  void set_time_sec(double t) { time_sec_ = t; }
  double time_sec() const { return time_sec_; }

  void set_ocean_enabled(bool on) { params_.ocean_enabled = on; }
  void set_cloud_enabled(bool on) { params_.cloud_enabled = on; }
  bool ocean_enabled() const { return params_.ocean_enabled; }
  bool cloud_enabled() const { return params_.cloud_enabled; }

  // Sync OceanSystem quality from AtmosphereParams::quality.
  void sync_systems_from_params();

  // Seed WindNoise / WaveFromWind / CloudNoise / SeaMaskFromLand so demo and
  // self-test have visible fields without external files. Does not flip
  // enable flags — call enable_demo() for that.
  void seed_procedural_baseline(const FieldGrid& grid,
                                const std::vector<LonLatRing>* land_rings);

  // Seed procedural baseline + turn ocean and cloud on (unit / self-test).
  void enable_demo(const FieldGrid& grid,
                   const std::vector<LonLatRing>* land_rings);

 private:
  AtmosphereParams params_;
  FieldStore field_store_;
  OceanSystem ocean_system_;
  CloudSystem cloud_system_;
  double time_sec_ = 0.0;
};

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_ENVIRONMENT_H_

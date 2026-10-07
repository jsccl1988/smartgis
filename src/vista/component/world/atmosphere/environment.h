// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_ATMOSPHERE_ENVIRONMENT_H_
#define VISTA_COMPONENT_WORLD_ATMOSPHERE_ENVIRONMENT_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "vista/component/world/atmosphere/atmosphere_params.h"
#include "vista/component/world/atmosphere/cloud/cloud_system.h"
#include "vista/component/world/atmosphere/contour/contour_sheet.h"
#include "vista/component/world/atmosphere/field/field_channel.h"
#include "vista/component/world/atmosphere/field/field_ingest.h"
#include "vista/component/world/atmosphere/field/field_store.h"
#include "vista/component/world/atmosphere/ocean/ocean_system.h"
#include "vista/vista_export.h"
#include "vista/domain/domain.h"
#include "vista/terrain/dem/mask/land_mask.h"

namespace vista {
namespace atmosphere {

// Session holder for atmosphere time, sun/quality params, shared FieldStore,
// and the Ocean/Cloud systems that drive GPU passes. Mounted optionally on
// MapScene / Scene3dController (not NodeKind). Ocean/cloud default off.
// Environment is the kAtmosphere DomainSession; it does not inherit
// DomainSession, so World includes stay unchanged.
class VISTA_EXPORT Environment {
 public:
  Environment();
  ~Environment();

  DomainKind kind() const { return DomainKind::kAtmosphere; }

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

  ContourSheet& contour_sheet() { return contour_sheet_; }
  const ContourSheet& contour_sheet() const { return contour_sheet_; }

  // Simulation / animation clock (seconds). Hosts advance this each frame.
  void set_time_sec(double t) { time_sec_ = t; }
  double time_sec() const { return time_sec_; }

  void set_ocean_enabled(bool on) { params_.ocean_enabled = on; }
  void set_cloud_enabled(bool on) { params_.cloud_enabled = on; }
  void set_sky_enabled(bool on);
  void set_fog_enabled(bool on);
  void set_contour_enabled(bool on) { params_.contour_enabled = on; }
  bool ocean_enabled() const { return params_.ocean_enabled; }
  bool cloud_enabled() const { return params_.cloud_enabled; }
  bool sky_enabled() const;
  bool fog_enabled() const;
  bool contour_enabled() const { return params_.contour_enabled; }

  // Rebuild ContourSheet from FieldStore |channel| using params_ contour knobs.
  // |dem_meters| optional same-size DEM (nullptr = flat base + dem_offset).
  bool rebuild_contour_sheet(FieldChannel channel, const FieldGrid& grid,
                             const float* dem_meters = nullptr);

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

  // Load External GeoTIFF (or GDAL-openable) time series into field_store_.
  // On success, session clock is set to times[0]. Views/showcase may wrap
  // this behind --atmosphere-fields= (Phase 1.2 host lane).
  bool load_external_series(FieldChannel channel, const char* const* paths,
                            const double* times, std::size_t count,
                            const FieldIngestOptions& opts = {});

  // Min/max over timed slices of |channel| in field_store_ (any kind/priority).
  bool timed_field_range(FieldChannel channel, double* out_min,
                         double* out_max) const;

  // Session time scrub: set absolute clock or advance by dt (seconds).
  void scrub_time_sec(double t) { set_time_sec(t); }
  void advance_time_sec(double dt_sec) { set_time_sec(time_sec_ + dt_sec); }

  // Clamp session clock into timed_field_range(|channel|) when a range exists.
  // Returns false if no timed slices (clock unchanged).
  bool clamp_time_to_field(FieldChannel channel);

 private:
  AtmosphereParams params_;
  FieldStore field_store_;
  OceanSystem ocean_system_;
  CloudSystem cloud_system_;
  ContourSheet contour_sheet_;
  double time_sec_ = 0.0;
};

// Allocate/free Environment inside vista.dll so ContourColorScale STL members
// are not new'd in a consumer EXE and deleted across the DLL boundary.
VISTA_EXPORT void destroy_environment(Environment* p);
struct EnvironmentDeleter {
  void operator()(Environment* p) const { destroy_environment(p); }
};
using EnvironmentPtr = std::unique_ptr<Environment, EnvironmentDeleter>;
VISTA_EXPORT EnvironmentPtr create_environment();

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_ATMOSPHERE_ENVIRONMENT_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_PROJECT_PASS_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_PROJECT_PASS_H_

#include "vista/component/world/atmosphere/atmosphere_params.h"
#include "vista/component/world/atmosphere/cloud/cloud_system.h"
#include "vista/component/world/atmosphere/ocean/ocean_system.h"
#include "vista/pass/world/atmosphere/fog/fog_pass.h"
#include "vista/pass/world/atmosphere/ocean/ocean_pass.h"
#include "vista/pass/world/atmosphere/sky/sky_pass.h"
#include "vista/vista_export.h"

namespace vista {
namespace atmosphere {

// Session-free field → pass POD projection. Content supplies orbit metrics
// (from OrbitGeoFrame) and product look flags; GisScene land rings stay in
// AtmosphereSession seed only.

// Orbit XZ skirt around the active lon/lat extent (same units as DEM mesh).
struct OrbitPatchRect {
  float min_x = 0.f;
  float max_x = 0.f;
  float min_z = 0.f;
  float max_z = 0.f;
  float sea_level_y = 0.f;
  float pad = 0.f;
};

// Product ocean albedo / Hs clamp family (not FieldStore).
enum class OceanLookPreset {
  kInteractive = 0,
  kLegacyStereo = 1,
};

// Cloud deck / slab in orbit units after GIS meters → orbit conversion.
struct CloudOrbitDeck {
  float cover = 0.5f;
  float base_m = 0.f;
  float top_m = 0.f;
  float half_x = 0.f;
  float half_z = 0.f;
  float deck_y = 0.f;
  float base_y = 0.f;
  float top_y = 0.f;
};

// Spectrum (GIS meters Hs already converted to |hs_orbit|) + orbit rect →
// OceanDrawParams. No GisScene / CommandList.
VISTA_EXPORT OceanDrawParams project_ocean_draw_params(
    const OceanSpectrumParams& spectrum, float hs_orbit,
    const OrbitPatchRect& patch, OceanLookPreset look);

// CloudSample + orbit patch → deck/slab knobs. |lift_orbit| / |thick_orbit|
// are raw meters_to_orbit_y results; clamps live here.
VISTA_EXPORT CloudOrbitDeck project_cloud_orbit_deck(
    const CloudSample& sample, const OrbitPatchRect& patch, float lift_orbit,
    float thick_orbit);

// Analytical sky palette for flat Scene3D vs globe splash.
VISTA_EXPORT SkyDrawParams project_sky_draw_params(bool globe_enabled);

// Floor sun elevation so daytime China orbit never samples a magenta mid-band.
VISTA_EXPORT float project_sky_sun_elevation(float sun_elevation_rad);

// Fog knobs from AtmosphereParams + sea level; tint from |sky| (caller zeros
// sun_glow when sampling horizon to avoid corona wash).
VISTA_EXPORT FogDrawParams project_fog_draw_params(const AtmosphereParams& p,
                                                   float sea_level_y,
                                                   const SkyDrawParams& sky);

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_PROJECT_PASS_H_

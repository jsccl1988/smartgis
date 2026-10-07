// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_CONSTANTS_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace vista {

// GPU byte layout for GlobeCB (register b1). Lit / eye fields for the PS.
// View/proj stay on CameraCB (b0) via camera_slot=0 — same path as sat_cloud
// and flat DEM; FlyCube uploads those matrices from draw.camera_* every draw.
struct GlobeConstants {
  float sun_x = 0.f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  float ambient = 0.42f;
  float intensity = 0.55f;
  float ocean_r = 0.04f;
  float ocean_g = 0.16f;
  float ocean_b = 0.38f;
  float eye_x = 0.f;
  float eye_y = 0.f;
  float eye_z = 3.f;
  float atmos_strength = 1.0f;
};

static_assert(sizeof(GlobeConstants) == 48, "GlobeCB is 48 bytes");
static_assert(sizeof(GlobeConstants) % 16 == 0,
              "GlobeCB size must be a multiple of 16");

// Satellite cloud shell CB (register b1).
struct SatCloudConstants {
  float opacity = 0.72f;
  float soft_edge = 0.15f;
  float time_sec = 0.f;
  float pad = 0.f;
};

static_assert(sizeof(SatCloudConstants) == 16, "SatCloudCB is 16 bytes");

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_GLOBE_CONSTANTS_H_

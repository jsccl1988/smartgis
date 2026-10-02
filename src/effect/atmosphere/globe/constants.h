// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_GLOBE_CONSTANTS_H_
#define EFFECT_ATMOSPHERE_GLOBE_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace effect {
namespace atmosphere {

// GPU byte layout for GlobeCB (register b1). Matches HLSL in globe/hlsl.h.
struct GlobeConstants {
  float sun_x = 0.f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  float ambient = 0.35f;
  float intensity = 1.15f;
  float ocean_r = 0.05f;
  float ocean_g = 0.14f;
  float ocean_b = 0.32f;
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

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_GLOBE_CONSTANTS_H_

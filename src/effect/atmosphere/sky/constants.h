// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_SKY_CONSTANTS_H_
#define EFFECT_ATMOSPHERE_SKY_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace effect {
namespace atmosphere {

// GPU byte layout of the sky graphics cbuffer (HLSL SkyCB, register b1).
// Matches sample_sky_rgb knobs plus camera eye for view-ray sampling.
struct SkyConstants {
  float sun_x;
  float sun_y;
  float sun_z;
  // 0 = daytime Rayleigh; 1 = procedural starfield (maps SkyDrawParams::space_blend).
  float space_blend;
  float zenith_r;
  float zenith_g;
  float zenith_b;
  float horizon_r;
  float horizon_g;
  float horizon_b;
  float sunset_r;
  float sunset_g;
  float sunset_b;
  float sun_glow;
  float cam_x;
  float cam_y;
  float cam_z;
  float pad;
  // HLSL cbuffers round size up to 16 bytes; keep C++ layout identical.
  float pad1;
  float pad2;
};

static_assert(sizeof(SkyConstants) % 16 == 0, "SkyCB size must be a multiple of 16");
static_assert(sizeof(SkyConstants) == 80, "SkyCB is 80 bytes");
static_assert(offsetof(SkyConstants, cam_x) == 56,
              "SkyCB camera row follows sun/colors/glow");

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_SKY_CONSTANTS_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_ATMOSPHERE_FOG_CONSTANTS_H_
#define VISTA_PASS_ATMOSPHERE_FOG_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace vista {

// GPU byte layout of the fog graphics cbuffer (HLSL FogCB, register b1).
struct FogConstants {
  float density;
  float height_falloff;
  float base_height;
  float visibility;
  float color_r;
  float color_g;
  float color_b;
  float max_opacity;
  float cam_x;
  float cam_y;
  float cam_z;
  // 1 when a depth SRV is bound; 0 selects CameraCB far-ray only.
  float use_depth;
};

static_assert(sizeof(FogConstants) == 48, "FogCB is 48 bytes");
static_assert(offsetof(FogConstants, cam_x) == 32,
              "FogCB camera row follows eight floats");
static_assert(offsetof(FogConstants, use_depth) == 44,
              "FogCB use_depth packs the last float");

}  // namespace vista

#endif  // VISTA_PASS_ATMOSPHERE_FOG_CONSTANTS_H_

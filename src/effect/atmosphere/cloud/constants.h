// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef EFFECT_ATMOSPHERE_CLOUD_CONSTANTS_H_
#define EFFECT_ATMOSPHERE_CLOUD_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace effect {
namespace atmosphere {

// GPU byte layout of the cloud graphics cbuffer (HLSL CloudCB, register b1).
struct CloudConstants {
  float sun_x;
  float sun_y;
  float sun_z;
  float cover;
  float base_m;
  float top_m;
  float extinction;
  float steps;
  float cam_x;
  float cam_y;
  float cam_z;
  float pad0;
};

static_assert(sizeof(CloudConstants) == 48, "CloudCB is 48 bytes");
static_assert(offsetof(CloudConstants, cam_x) == 32,
              "CloudCB camera row follows eight floats");

}  // namespace atmosphere
}  // namespace effect

#endif  // EFFECT_ATMOSPHERE_CLOUD_CONSTANTS_H_

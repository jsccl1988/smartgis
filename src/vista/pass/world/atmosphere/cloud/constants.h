// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_CONSTANTS_H_
#define VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace vista {

// GPU byte layout of the cloud graphics cbuffer (CloudCB in vs/ps_cloud.hlsl,
// register b1).
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
  // World-space snap cell for density; 0 = full detail (quality >= 1).
  float density_cell;
  // Orbit deck half-extents for cover-texture UV (XZ centered at origin).
  float deck_hx;
  float deck_hz;
  // 1 when cover_map SRV is bound; else density uses scalar cover only.
  float has_cover_tex;
  float pad0;
};

static_assert(sizeof(CloudConstants) == 64, "CloudCB is 64 bytes");
static_assert(offsetof(CloudConstants, cam_x) == 32,
              "CloudCB camera row follows eight floats");
static_assert(offsetof(CloudConstants, density_cell) == 44,
              "CloudCB density_cell before deck row");
static_assert(offsetof(CloudConstants, deck_hx) == 48,
              "CloudCB deck extents start at byte 48");

}  // namespace vista

#endif  // VISTA_PASS_WORLD_ATMOSPHERE_CLOUD_CONSTANTS_H_

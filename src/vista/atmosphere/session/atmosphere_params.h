// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_
#define GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

namespace vista {
namespace atmosphere {

// Session-level atmosphere toggles and quality knobs for ocean / cloud / sky /
// fog passes. Host projects these into vista GPU atmosphere POD.
struct AtmosphereParams {
  // Sun direction in world/view conventions used by Scene3dController (radians).
  // ~32 deg: mid-tier sky with readable Mie horizon warmth (not washed noon).
  float sun_azimuth_rad = 0.55f;
  float sun_elevation_rad = 0.56f;

  // Cloud raymarch step budget (higher = denser samples). Ocean FFT size hint.
  int quality = 1;

  bool ocean_enabled = false;
  bool cloud_enabled = false;
  bool sky_enabled = false;
  bool fog_enabled = false;

  // Fog visibility knobs (orbit-normalized units; projected to FogDrawParams).
  // Soft defaults: aerial haze without washing DEM hypsometric greens.
  float fog_density = 0.09f;
  float fog_visibility = 3.6f;
  float fog_height_falloff = 1.35f;
  float fog_max_opacity = 0.22f;
};

}  // namespace atmosphere
}  // namespace vista

#endif  // GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

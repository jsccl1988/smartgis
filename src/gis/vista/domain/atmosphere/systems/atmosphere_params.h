// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_
#define GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

namespace gis {
namespace atmosphere {

// Session-level atmosphere toggles and quality knobs for ocean / cloud / sky /
// fog passes. Host projects these into effect::atmosphere POD.
struct AtmosphereParams {
  // Sun direction in world/view conventions used by Scene3dController (radians).
  float sun_azimuth_rad = 0.0f;
  float sun_elevation_rad = 0.785398163f;  // ~45 deg

  // Cloud raymarch step budget (higher = denser samples). Ocean FFT size hint.
  int quality = 1;

  bool ocean_enabled = false;
  bool cloud_enabled = false;
  bool sky_enabled = false;
  bool fog_enabled = false;

  // Fog visibility knobs (orbit-normalized units; projected to FogDrawParams).
  float fog_density = 0.04f;
  float fog_visibility = 6.0f;
  float fog_height_falloff = 1.2f;
  float fog_max_opacity = 0.55f;
};

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

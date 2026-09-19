// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_
#define GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

namespace gis {
namespace atmosphere {

// Session-level atmosphere toggles and quality knobs for ocean / cloud passes.
struct AtmosphereParams {
  // Sun direction in world/view conventions used by Scene3dController (radians).
  float sun_azimuth_rad = 0.0f;
  float sun_elevation_rad = 0.785398163f;  // ~45 deg

  // Cloud raymarch step budget (higher = denser samples). Ocean FFT size hint.
  int quality = 1;

  bool ocean_enabled = false;
  bool cloud_enabled = false;
};

}  // namespace atmosphere
}  // namespace gis

#endif  // GIS_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

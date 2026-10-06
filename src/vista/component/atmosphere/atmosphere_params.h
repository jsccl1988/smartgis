// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_ATMOSPHERE_ATMOSPHERE_PARAMS_H_
#define VISTA_COMPONENT_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

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
  // Origin-style stacked contour sheet (curves + TIN) above DEM.
  bool contour_enabled = false;
  bool contour_curves = true;
  bool contour_surface = true;
  // Screen-ortho side color scale (jet bar + tick / title text anchors).
  bool contour_color_scale = true;
  // Lift (mesh Y meters) so the sheet clears terrain for presentation.
  float contour_dem_offset_m = 500.f;
  // Field value range → additional sheet undulation (meters). Milder than
  // true GIS meters so China-orbit drapes read as gentle relief, not cliffs.
  float contour_value_to_meters = 160.f;
  // Multiplier on optional DEM meters before dem_offset + undulation.
  float contour_dem_vert_exag = 0.35f;
  float contour_surface_alpha = 0.55f;
  // Color-scale layout (viewport fractions; right side by default).
  float contour_scale_margin = 0.03f;
  float contour_scale_bar_width = 0.028f;
  float contour_scale_bar_height = 0.48f;
  int contour_scale_tick_count = 6;

  // Fog visibility knobs (orbit-normalized units; projected to FogDrawParams).
  // Soft defaults: aerial haze without washing DEM hypsometric greens.
  float fog_density = 0.09f;
  float fog_visibility = 3.6f;
  float fog_height_falloff = 1.35f;
  float fog_max_opacity = 0.22f;
};

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_ATMOSPHERE_ATMOSPHERE_PARAMS_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/atmosphere/project_pass.h"

#include <algorithm>

namespace vista {
namespace atmosphere {

OceanDrawParams project_ocean_draw_params(const OceanSpectrumParams& spectrum,
                                          float hs_orbit,
                                          const OrbitPatchRect& patch,
                                          OceanLookPreset look) {
  OceanDrawParams draw;
  // Wave height: GIS meters already converted to orbit Y by the caller.
  draw.significant_wave_height = (std::max)(0.01f, hs_orbit);
  draw.mean_direction_rad = spectrum.mean_direction_rad;
  draw.wind_speed = spectrum.wind_speed;
  draw.wind_direction_rad = spectrum.wind_direction_rad;
  draw.fft_size = spectrum.fft_size;
  draw.use_gerstner_fallback = spectrum.use_gerstner_fallback;
  draw.use_jonswap = spectrum.use_jonswap;
  draw.chop = spectrum.chop;
  draw.jonswap_gamma = spectrum.jonswap_gamma;

  const float pad = patch.pad;
  const float min_x = patch.min_x - pad;
  const float max_x = patch.max_x + pad;
  const float min_z = patch.min_z - pad;
  const float max_z = patch.max_z + pad;
  draw.patch_center_x = 0.5f * (min_x + max_x);
  draw.patch_center_z = 0.5f * (min_z + max_z);
  draw.patch_y = patch.sea_level_y;
  draw.patch_half_x = 0.5f * (max_x - min_x);
  draw.patch_half_z = 0.5f * (max_z - min_z);
  draw.patch_half_extent =
      (std::max)(draw.patch_half_x, draw.patch_half_z);

  if (look == OceanLookPreset::kLegacyStereo) {
    // Leftover stereo: calm light-blue shelf. Cap Hs so coastal DEM survives.
    draw.significant_wave_height =
        (std::min)((std::max)(draw.significant_wave_height, 0.01f), 0.035f);
    draw.use_gerstner_fallback = true;
    draw.prefer_gpu_fft = false;
    draw.chop = (std::min)((std::max)(draw.chop, 0.35f), 0.55f);
    draw.shininess = (std::max)(draw.shininess, 120.0f);
    draw.mesh_resolution = 33;
    draw.deep_r = 0.01f;
    draw.deep_g = 0.02f;
    draw.deep_b = 0.04f;
    draw.shallow_r = 0.22f;
    draw.shallow_g = 0.48f;
    draw.shallow_b = 0.62f;
    draw.fresnel_bias = 0.04f;
    draw.fresnel_power = 5.0f;
  } else {
    // China orbit span ~3.2: readable lip without swallowing DEM peaks.
    // Prefer Gerstner for interactive full-China (GPU FFT energy is tiny).
    draw.significant_wave_height =
        (std::max)((std::min)(draw.significant_wave_height * 4.5f, 0.22f),
                   0.10f);
    draw.use_gerstner_fallback = true;
    draw.prefer_gpu_fft = false;
    draw.chop = (std::max)(draw.chop, 1.15f);
    draw.shininess = (std::max)(draw.shininess, 180.0f);
    draw.mesh_resolution = 33;
    draw.deep_r = 0.02f;
    draw.deep_g = 0.07f;
    draw.deep_b = 0.18f;
    draw.shallow_r = 0.06f;
    draw.shallow_g = 0.22f;
    draw.shallow_b = 0.32f;
    draw.fresnel_bias = 0.03f;
    draw.fresnel_power = 6.0f;
  }
  return draw;
}

CloudOrbitDeck project_cloud_orbit_deck(const CloudSample& sample,
                                        const OrbitPatchRect& patch,
                                        float lift_orbit, float thick_orbit) {
  CloudOrbitDeck deck;
  deck.base_m = sample.base_m;
  deck.top_m = sample.top_m;
  // Broken deck: visible but soft enough that landish greens survive.
  deck.cover = (std::max)(0.38f, (std::min)(sample.cover, 0.62f));

  const float pad = patch.pad;
  deck.half_x = 0.5f * (patch.max_x - patch.min_x) + pad;
  deck.half_z = 0.5f * (patch.max_z - patch.min_z) + pad;

  // Keep a thin deck just above the terrain (full GIS cloud base would sit
  // several orbit-units up as a gray card).
  const float lift = (std::max)(0.22f, (std::min)(lift_orbit, 0.62f));
  const float thick = (std::max)(0.16f, (std::min)(thick_orbit, 0.36f));
  deck.base_y = patch.sea_level_y + lift;
  deck.top_y = deck.base_y + thick;
  deck.deck_y = 0.5f * (deck.base_y + deck.top_y);
  return deck;
}

SkyDrawParams project_sky_draw_params(bool globe_enabled) {
  SkyDrawParams sky;
  if (globe_enabled) {
    // Product splash / Google-Earth path: deep-space starfield, not Rayleigh.
    // Negative dome_radius selects space_blend in SkyPass (no POD growth).
    sky.dome_radius = -40.0f;
    sky.zenith_r = 0.008f;
    sky.zenith_g = 0.010f;
    sky.zenith_b = 0.028f;
    sky.horizon_r = 0.012f;
    sky.horizon_g = 0.014f;
    sky.horizon_b = 0.040f;
    sky.sunset_r = sky.horizon_r;
    sky.sunset_g = sky.horizon_g;
    sky.sunset_b = sky.horizon_b;
    sky.sun_glow_strength = 0.55f;
  } else {
    // Industry mid-tier analytical dome: deep Rayleigh zenith, cool haze
    // horizon. Collapse sunset into horizon so screen-space ground never
    // samples a magenta sunset leg.
    sky.dome_radius = 40.0f;
    sky.zenith_r = 0.04f;
    sky.zenith_g = 0.16f;
    sky.zenith_b = 0.86f;
    sky.horizon_r = 0.42f;
    sky.horizon_g = 0.64f;
    sky.horizon_b = 0.90f;
    sky.sunset_r = sky.horizon_r;
    sky.sunset_g = sky.horizon_g;
    sky.sunset_b = sky.horizon_b;
    sky.sun_glow_strength = 0.18f;
  }
  return sky;
}

float project_sky_sun_elevation(float sun_elevation_rad) {
  return (std::max)(sun_elevation_rad, 0.72f);
}

FogDrawParams project_fog_draw_params(const AtmosphereParams& p,
                                      float sea_level_y,
                                      const SkyDrawParams& sky) {
  FogDrawParams fog;
  // Soft aerial haze on terrain only. Cap opacity so hypsometric greens
  // still pass showcase landish gates.
  fog.density = (std::max)((std::min)(p.fog_density, 0.08f), 0.03f);
  // China orbit frame span ~3.2: keep haze visible without washing terrain.
  fog.visibility = (std::max)(2.8f, (std::min)(p.fog_visibility, 5.0f));
  fog.height_falloff = p.fog_height_falloff;
  fog.max_opacity = (std::min)((std::max)(p.fog_max_opacity, 0.06f), 0.12f);
  fog.base_height = sea_level_y;

  // Tint haze toward the analytical sky horizon. Zero sun_glow for the tint
  // sample — a fixed horizon ray can align with the sun and pick up disk /
  // corona, blowing fog to near-white.
  SkyDrawParams tint = sky;
  tint.sun_glow_strength = 0.f;
  float hr = fog.color_r;
  float hg = fog.color_g;
  float hb = fog.color_b;
  SkyPass::sample_sky_rgb(tint, 0.f, 0.05f, 1.f, &hr, &hg, &hb);
  fog.color_r = hr;
  fog.color_g = hg;
  fog.color_b = hb;
  return fog;
}

}  // namespace atmosphere
}  // namespace vista

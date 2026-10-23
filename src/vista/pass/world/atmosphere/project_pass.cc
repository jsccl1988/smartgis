// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/atmosphere/project_pass.h"

#include <algorithm>

namespace vista {
namespace atmosphere {
namespace {

int ocean_mesh_resolution_for_quality(int quality) {
  // Displaceable verts/edge. Independent of FFT size; raise with quality for
  // Design B interactive look without blowing Hs over DEM.
  if (quality <= 0) {
    return 33;
  }
  if (quality == 1) {
    return 65;
  }
  if (quality == 2) {
    return 97;
  }
  return 129;
}

}  // namespace

OceanDrawParams project_ocean_draw_params(const OceanSpectrumParams& spectrum,
                                          float hs_orbit,
                                          const OrbitPatchRect& patch,
                                          OceanLookPreset look,
                                          int quality) {
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
  } else if (quality <= 0) {
    // Quality 0: calm Gerstner shelf (same DEM-safe Hs cap).
    draw.significant_wave_height =
        (std::min)((std::max)(draw.significant_wave_height, 0.01f), 0.035f);
    draw.use_gerstner_fallback = true;
    draw.prefer_gpu_fft = false;
    draw.chop = (std::min)((std::max)(draw.chop, 0.35f), 0.55f);
    draw.shininess = (std::max)(draw.shininess, 160.0f);
    draw.mesh_resolution = ocean_mesh_resolution_for_quality(0);
    draw.deep_r = 0.02f;
    draw.deep_g = 0.07f;
    draw.deep_b = 0.18f;
    draw.shallow_r = 0.06f;
    draw.shallow_g = 0.22f;
    draw.shallow_b = 0.32f;
    draw.fresnel_bias = 0.035f;
    draw.fresnel_power = 5.5f;
  } else {
    // Design B interactive: GPU FFT when spectrum allows; keep DEM-safe Hs.
    // Sea-mask discard stays on OceanPass (caller fills kSeaMask).
    draw.significant_wave_height =
        (std::min)((std::max)(draw.significant_wave_height, 0.01f), 0.035f);
    // Prefer GPU FFT for quality ≥ 1; spectrum.use_gerstner_fallback only when
    // fft_size < 16 (quality 0). Do not force Gerstner here.
    draw.prefer_gpu_fft = true;
    draw.use_gerstner_fallback = (draw.fft_size < 16);
    // Modest chop: readable fold without sliding verts over land quads.
    draw.chop = (std::min)((std::max)(draw.chop, 0.45f), 0.85f);
    draw.shininess = (std::max)(draw.shininess, 220.0f);
    draw.mesh_resolution = ocean_mesh_resolution_for_quality(quality);
    draw.deep_r = 0.015f;
    draw.deep_g = 0.06f;
    draw.deep_b = 0.20f;
    draw.shallow_r = 0.05f;
    draw.shallow_g = 0.24f;
    draw.shallow_b = 0.36f;
    // Stronger Fresnel rim without blowing a cyan sheet.
    draw.fresnel_bias = 0.045f;
    draw.fresnel_power = 4.5f;
  }
  return draw;
}

CloudOrbitDeck project_cloud_orbit_deck(const CloudSample& sample,
                                        const OrbitPatchRect& patch,
                                        float lift_orbit, float thick_orbit) {
  CloudOrbitDeck deck;
  deck.base_m = sample.base_m;
  deck.top_m = sample.top_m;
  // Broken deck: field cover drives texture modulation; keep a soft floor /
  // ceiling so DEM greens survive under sparse cover.
  deck.cover = (std::max)(0.28f, (std::min)(sample.cover, 0.72f));

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
    // Design B Bruneton-lite: deeper Rayleigh zenith, cooler horizon haze.
    // Still analytical — no Hillaire LUT tables.
    sky.dome_radius = 40.0f;
    sky.zenith_r = 0.03f;
    sky.zenith_g = 0.14f;
    sky.zenith_b = 0.92f;
    sky.horizon_r = 0.36f;
    sky.horizon_g = 0.58f;
    sky.horizon_b = 0.88f;
    sky.sunset_r = sky.horizon_r;
    sky.sunset_g = sky.horizon_g;
    sky.sunset_b = sky.horizon_b;
    sky.sun_glow_strength = 0.24f;
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
  // Design B: stronger depth-aware aerial haze; still capped so hypsometric
  // greens survive landish gates (no near-white wash).
  fog.density = (std::max)((std::min)(p.fog_density, 0.11f), 0.04f);
  fog.visibility = (std::max)(2.4f, (std::min)(p.fog_visibility, 4.5f));
  fog.height_falloff = (std::max)(p.fog_height_falloff, 1.15f);
  fog.max_opacity = (std::min)((std::max)(p.fog_max_opacity, 0.10f), 0.22f);
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

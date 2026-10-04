// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/fly/globe_fly.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "vista/atmosphere/globe/globe_pass.h"

#include <algorithm>
#include <cmath>

namespace app {
namespace detail {
namespace {

// Smoothstep for Google-Earth-like dolly easing (no overshoot).
float globe_fly_ease(float t) {
  t = (std::max)(0.f, (std::min)(1.f, t));
  return t * t * (3.f - 2.f * t);
}

}  // namespace

// Cinematic Google-Earth / product-splash path:
//   0.00–0.12  deep-space hold on China
//   0.12–0.40  ease space → high altitude (BMP capture ~0.48)
//   0.40–0.52  high hold + slow yaw
//   0.52–0.78  dive onto Tibet / Himalaya DEM + pitch toward plateau skim
//   0.78–1.00  near-earth skim over Tibet with DEM height-follow undulation
void apply_globe_flythrough(content::OrbitFrame* orbit,
                            float t01,
                            float china_yaw,
                            float china_pitch,
                            const vista::GlobePass* globe,
                            content::AtmosphereSession* session) {
  if (!orbit) {
    return;
  }
  // Near-earth: thin/disable sat cloud so DEM normals + Lambert dominate.
  if (session) {
    session->set_sat_cloud_enabled(t01 < 0.70f);
  }
  // Stay outside exaggerated DEM peaks (~1.10 R) + sat-cloud shell (~1.14).
  orbit->set_dolly_limits(1.14f, 12.f);
  constexpr float kSpaceDist = 12.f;
  constexpr float kHighDist = 2.85f;
  constexpr float kSurfaceClear = 0.10f;
  constexpr float kSkimDistFloor = 1.20f;
  // Tibet / Himalaya orbit aim (~90E, 32N) — same ECEF→yaw/pitch as China seed.
  constexpr float kTibetLon = 90.f * 3.14159265f / 180.f;
  constexpr float kTibetLat = 32.f * 3.14159265f / 180.f;
  const float tibet_cl = std::cos(kTibetLat);
  const float tibet_yaw =
      std::atan2(tibet_cl * std::sin(kTibetLon), tibet_cl * std::cos(kTibetLon));
  const float tibet_pitch =
      std::asin((std::max)(-1.f, (std::min)(1.f, std::sin(kTibetLat))));
  // Mild horizon flatten still keeps the camera over the plateau (~23°), not
  // SE Asia (~12°) where china_pitch*0.22 previously aimed.
  const float skim_pitch = tibet_pitch * 0.72f;
  // High-hold ends at china_yaw + 0.16; dive must unwind that eastward drift.
  constexpr float kHighYawBias = 0.16f;
  float dist = kHighDist;
  float yaw = china_yaw;
  float pitch = china_pitch;
  if (t01 <= 0.12f) {
    dist = kSpaceDist;
  } else if (t01 < 0.40f) {
    const float u = globe_fly_ease((t01 - 0.12f) / 0.28f);
    dist = kSpaceDist + (kHighDist - kSpaceDist) * u;
    yaw = china_yaw + 0.06f * u;
  } else if (t01 < 0.52f) {
    dist = kHighDist;
    const float u = (t01 - 0.40f) / 0.12f;
    yaw = china_yaw + 0.06f + 0.10f * u;
  } else if (t01 < 0.78f) {
    const float u = globe_fly_ease((t01 - 0.52f) / 0.26f);
    // Dive look ray onto western China / Tibet (not Pacific / SE Asia).
    const float dive_start_yaw = china_yaw + kHighYawBias;
    yaw = dive_start_yaw + (tibet_yaw - dive_start_yaw) * u;
    pitch = china_pitch + (skim_pitch - china_pitch) * u;
    float surface = 1.0f + kSurfaceClear;
    if (globe && globe->has_surface()) {
      const float lon_deg =
          90.f + (105.f - 90.f) * (1.f - u);  // China center → Tibet
      const float lat_deg = 35.f + (32.f - 35.f) * u;
      surface = globe->surface_radius(lon_deg, lat_deg) + kSurfaceClear;
    }
    dist = kHighDist + (surface - kHighDist) * u;
  } else {
    const float u = globe_fly_ease((t01 - 0.78f) / 0.22f);
    // Horizon skim centered on Tibet / Himalaya with slight east drift.
    yaw = tibet_yaw + 0.05f * u;
    pitch = skim_pitch + 0.04f * u;
    float surface = 1.0f + kSurfaceClear;
    if (globe && globe->has_surface()) {
      const float lon_deg = 90.f + 6.f * u;
      const float lat_deg = 32.f - 1.5f * u;
      surface = globe->surface_radius(lon_deg, lat_deg) + kSurfaceClear;
    }
    // Closer skim for Lambert DEM relief; stay above dolly min (1.14).
    dist = (std::max)(kSkimDistFloor, surface);
  }
  orbit->set_yaw(yaw);
  orbit->set_pitch(pitch);
  orbit->set_distance(dist);
}

}  // namespace detail
}  // namespace app

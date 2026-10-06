// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/fly/globe_fly.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "vista/pass/atmosphere/globe/globe_pass.h"

#include <algorithm>
#include <cmath>

namespace plugin {
namespace {

float globe_fly_ease(float t) {
  t = (std::max)(0.f, (std::min)(1.f, t));
  return t * t * (3.f - 2.f * t);
}

void look_yaw_pitch(float lon_deg, float lat_deg, float* yaw, float* pitch) {
  constexpr float kDeg = 3.14159265f / 180.f;
  const float lon = lon_deg * kDeg;
  const float lat = lat_deg * kDeg;
  const float cl = std::cos(lat);
  *yaw = std::atan2(cl * std::sin(lon), cl * std::cos(lon));
  *pitch = std::asin((std::max)(-1.f, (std::min)(1.f, std::sin(lat))));
}

void lerp_ll(float lon0, float lat0, float lon1, float lat1, float u,
             float* lon, float* lat) {
  *lon = lon0 + (lon1 - lon0) * u;
  *lat = lat0 + (lat1 - lat0) * u;
}

// Layer policy for the four cinematic beats. Always toggles ocean off when
// rewinding out of the sea skim so parked DEM frames are not Gerstner-tinted.
void apply_globe_fly_layers(content::AtmosphereSession* session, float t01) {
  if (!session) {
    return;
  }
  session->set_cloud_enabled(false);
  if (t01 < 0.15f) {
    session->set_sat_cloud_enabled(true);
    session->set_ocean_enabled(false);
  } else if (t01 < 0.42f) {
    session->set_sat_cloud_enabled(true);
    session->set_ocean_enabled(false);
  } else if (t01 < 0.85f) {
    session->set_sat_cloud_enabled(t01 < 0.55f);
    session->set_ocean_enabled(false);
  } else {
    session->set_sat_cloud_enabled(false);
    session->set_ocean_enabled(true);
  }
}

}  // namespace

void world3d_china_aim_yaw_pitch(float* yaw, float* pitch) {
  if (!yaw || !pitch) {
    return;
  }
  look_yaw_pitch(105.f, 35.f, yaw, pitch);
}

void apply_world3d_globe_flythrough(content::OrbitFrame* orbit,
                                    float t01,
                                    float china_yaw,
                                    float china_pitch,
                                    const vista::GlobePass* globe,
                                    content::AtmosphereSession* session) {
  if (!orbit) {
    return;
  }
  apply_globe_fly_layers(session, t01);

  orbit->set_dolly_limits(1.12f, 12.f);
  constexpr float kSpaceDist = 12.f;
  constexpr float kHighDist = 2.85f;
  constexpr float kSurfaceClear = 0.055f;
  constexpr float kSkimDistFloor = 1.14f;

  float look_lon = 105.f;
  float look_lat = 35.f;
  float dist = kHighDist;
  float yaw = china_yaw;
  float pitch = china_pitch;

  if (t01 <= 0.12f) {
    dist = kSpaceDist;
    look_lon = 105.f;
    look_lat = 35.f;
  } else if (t01 < 0.40f) {
    const float u = globe_fly_ease((t01 - 0.12f) / 0.28f);
    dist = kSpaceDist + (kHighDist - kSpaceDist) * u;
    yaw = china_yaw + 0.06f * u;
    look_lon = 105.f;
    look_lat = 35.f;
  } else if (t01 < 0.52f) {
    dist = kHighDist;
    const float u = (t01 - 0.40f) / 0.12f;
    yaw = china_yaw + 0.06f + 0.10f * u;
    look_lon = 105.f;
    look_lat = 35.f;
  } else {
    float u_path = 0.f;
    if (t01 < 0.70f) {
      u_path = globe_fly_ease((t01 - 0.52f) / 0.18f);
      lerp_ll(78.f, 38.f, 92.f, 32.f, u_path, &look_lon, &look_lat);
    } else if (t01 < 0.82f) {
      u_path = globe_fly_ease((t01 - 0.70f) / 0.12f);
      lerp_ll(92.f, 32.f, 108.f, 34.f, u_path, &look_lon, &look_lat);
    } else if (t01 < 0.92f) {
      u_path = globe_fly_ease((t01 - 0.82f) / 0.10f);
      lerp_ll(108.f, 34.f, 118.f, 32.5f, u_path, &look_lon, &look_lat);
    } else {
      u_path = globe_fly_ease((t01 - 0.92f) / 0.08f);
      lerp_ll(118.f, 32.5f, 124.f, 31.2f, u_path, &look_lon, &look_lat);
    }
    float aim_yaw = china_yaw;
    float aim_pitch = china_pitch;
    look_yaw_pitch(look_lon, look_lat, &aim_yaw, &aim_pitch);
    const float skim_pitch = aim_pitch;
    if (t01 < 0.70f) {
      const float u = globe_fly_ease((t01 - 0.52f) / 0.18f);
      constexpr float kHighYawBias = 0.16f;
      const float dive_start_yaw = china_yaw + kHighYawBias;
      yaw = dive_start_yaw + (aim_yaw - dive_start_yaw) * u;
      pitch = china_pitch + (skim_pitch - china_pitch) * u;
      float surface = 1.0f + kSurfaceClear;
      if (globe && globe->has_surface()) {
        surface = globe->surface_radius(look_lon, look_lat) + kSurfaceClear;
      }
      dist = kHighDist + (surface - kHighDist) * u;
    } else {
      yaw = aim_yaw + 0.04f;
      pitch = skim_pitch;
      float surface = 1.0f + kSurfaceClear;
      if (globe && globe->has_surface()) {
        surface = globe->surface_radius(look_lon, look_lat) + kSurfaceClear;
      }
      dist = (std::max)(kSkimDistFloor, surface);
    }
  }
  if (session) {
    session->update_globe_detail_lod(dist, look_lon, look_lat);
  }
  orbit->set_yaw(yaw);
  orbit->set_pitch(pitch);
  orbit->set_distance(dist);
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/fly/globe_fly.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "vista/pass/world/atmosphere/globe/globe_pass.h"

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

// Match make_orbit_camera spherical mapping (yaw around +Y, pitch from XZ).
void ll_to_xyz(float lon_deg, float lat_deg, float radius, float* x, float* y,
               float* z) {
  float yaw = 0.f;
  float pitch = 0.f;
  look_yaw_pitch(lon_deg, lat_deg, &yaw, &pitch);
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);
  const float cy = std::cos(yaw);
  const float sy = std::sin(yaw);
  *x = radius * cp * sy;
  *y = radius * sp;
  *z = radius * cp * cy;
}

float surface_r(const vista::GlobePass* globe, float lon, float lat,
                float clearance) {
  const float base =
      (globe && globe->params().radius > 0.5f) ? globe->params().radius : 1.f;
  float r = base + clearance;
  if (globe && globe->has_surface()) {
    r = globe->surface_radius(lon, lat) + clearance;
  }
  return r;
}

// West China DEM → central mountains → east coast → East China Sea.
void sample_skim_path(float u01, float* lon, float* lat) {
  const float u = globe_fly_ease((std::max)(0.f, (std::min)(1.f, u01)));
  if (u < 0.22f) {
    lerp_ll(78.f, 38.5f, 92.f, 35.5f, u / 0.22f, lon, lat);
  } else if (u < 0.45f) {
    lerp_ll(92.f, 35.5f, 105.f, 34.2f, (u - 0.22f) / 0.23f, lon, lat);
  } else if (u < 0.68f) {
    lerp_ll(105.f, 34.2f, 116.5f, 32.8f, (u - 0.45f) / 0.23f, lon, lat);
  } else if (u < 0.86f) {
    lerp_ll(116.5f, 32.8f, 122.5f, 31.4f, (u - 0.68f) / 0.18f, lon, lat);
  } else {
    lerp_ll(122.5f, 31.4f, 128.5f, 30.2f, (u - 0.86f) / 0.14f, lon, lat);
  }
}

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
  } else if (t01 < 0.82f) {
    session->set_sat_cloud_enabled(t01 < 0.55f);
    session->set_ocean_enabled(false);
  } else {
    session->set_sat_cloud_enabled(false);
    session->set_ocean_enabled(true);
  }
}

// Horizontal-forward terrain-hug (正前方):
// eye above the DEM roof along the look segment, look target ahead at the
// same cruise radius (tangent). DEM relief scrolls under the horizon —
// not nadir look-at-origin.
void apply_forward_terrain_hug(content::OrbitFrame* orbit,
                               const vista::GlobePass* globe, float path_u,
                               float* out_lon, float* out_lat,
                               float* out_dist) {
  const float R =
      (globe && globe->params().radius > 0.5f) ? globe->params().radius : 1.f;
  // Absolute world units (same space as GlobePass::surface_radius).
  // Radial-up. Pure level aim (same r) grazes into starfield; hard look-down
  // near-clips cyan slabs. Mild down-pitch + tall clearance is the balance.
  const float kClear = 0.30f * R;
  const float kHugFloor = 1.30f * R;
  constexpr float kLookAheadPath = 0.20f;

  float lon = 105.f;
  float lat = 35.f;
  sample_skim_path(path_u, &lon, &lat);
  float lon_a = lon;
  float lat_a = lat;
  float ahead_u = (std::min)(1.f, path_u + kLookAheadPath);
  sample_skim_path(ahead_u, &lon_a, &lat_a);
  if (ahead_u - path_u < 0.03f) {
    sample_skim_path((std::max)(0.f, path_u - kLookAheadPath), &lon, &lat);
    sample_skim_path(path_u, &lon_a, &lat_a);
    ahead_u = path_u;
    path_u = (std::max)(0.f, path_u - kLookAheadPath);
  }

  // Roof of DEM under the look corridor (path + small lateral samples).
  float cruise = kHugFloor;
  constexpr int kRoofSamples = 24;
  for (int i = 0; i <= kRoofSamples; ++i) {
    const float u =
        path_u + (ahead_u - path_u) * (static_cast<float>(i) /
                                       static_cast<float>(kRoofSamples));
    float slon = lon;
    float slat = lat;
    sample_skim_path(u, &slon, &slat);
    cruise = (std::max)(cruise, surface_r(globe, slon, slat, kClear));
    cruise = (std::max)(cruise, surface_r(globe, slon + 0.6f, slat, kClear));
    cruise = (std::max)(cruise, surface_r(globe, slon - 0.6f, slat, kClear));
  }

  float ex = 0.f;
  float ey = 0.f;
  float ez = 0.f;
  float ax = 0.f;
  float ay = 0.f;
  float az = 0.f;
  ll_to_xyz(lon, lat, cruise, &ex, &ey, &ez);
  // Mild look-down: aim between the ahead roof and the surface so DEM fills
  // the lower FOV without underfoot near-clip slabs.
  float ahead_cruise =
      (std::max)(kHugFloor, surface_r(globe, lon_a, lat_a, kClear));
  ahead_cruise = (std::max)(ahead_cruise, cruise);
  const float ahead_surf = surface_r(globe, lon_a, lat_a, 0.06f * R);
  const float aim_r = ahead_surf * 0.35f + ahead_cruise * 0.65f;
  ll_to_xyz(lon_a, lat_a, aim_r, &ax, &ay, &az);
  float tx = ax;
  float ty = ay;
  float tz = az;

  float yaw = 0.f;
  float pitch = 0.f;
  look_yaw_pitch(lon, lat, &yaw, &pitch);
  orbit->set_dolly_limits(1.2f * R, 8.f * R);
  orbit->set_yaw(yaw);
  orbit->set_pitch(pitch);
  orbit->set_distance(cruise);
  orbit->set_forward_skim(ex, ey, ez, tx, ty, tz);

  if (out_lon) {
    *out_lon = lon;
  }
  if (out_lat) {
    *out_lat = lat;
  }
  if (out_dist) {
    *out_dist = cruise;
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
                                    content::AtmosphereSession* session,
                                    bool allow_forward_skim) {
  if (!orbit) {
    return;
  }
  apply_globe_fly_layers(session, t01);

  // Absolute world units matching GlobePass::surface_radius.
  // Space sits just above china_detail load (~3.2 R) so the space BMP keeps a
  // full-sphere ocean-blue global_terrain read without remesh, while still
  // filling the FOV (12 R left only a sky-blue limb sliver).
  const float R =
      (globe && globe->params().radius > 0.5f) ? globe->params().radius : 1.f;
  // ~2.6 R fills most of FOV_Y=1.35 while staying above china_detail (~2.4 R).
  const float kSpaceDist = 2.6f * R;
  const float kHighDist = 2.2f * R;
  const float kSurfaceClear = 0.018f * R;
  const float kDollyMin = 1.05f * R;
  const float kDollyMax = 8.f * R;
  constexpr float kHugStart = 0.60f;

  float look_lon = 105.f;
  float look_lat = 35.f;
  float dist = kHighDist;
  float yaw = china_yaw;
  float pitch = china_pitch;

  if (t01 < kHugStart || !allow_forward_skim) {
    orbit->clear_forward_skim();
  }

  if (t01 <= 0.12f) {
    orbit->set_dolly_limits(kDollyMin, kDollyMax);
    dist = kSpaceDist;
    look_lon = 105.f;
    look_lat = 35.f;
  } else if (t01 < 0.40f) {
    orbit->set_dolly_limits(kDollyMin, kDollyMax);
    const float u = globe_fly_ease((t01 - 0.12f) / 0.28f);
    dist = kSpaceDist + (kHighDist - kSpaceDist) * u;
    yaw = china_yaw + 0.06f * u;
    look_lon = 105.f;
    look_lat = 35.f;
  } else if (t01 < 0.50f) {
    // High China hold (suite park / landish score BMP).
    orbit->set_dolly_limits(kDollyMin, kDollyMax);
    dist = kHighDist;
    const float u = (t01 - 0.40f) / 0.10f;
    yaw = china_yaw + 0.06f + 0.10f * u;
    look_lon = 105.f;
    look_lat = 35.f;
  } else if (t01 < kHugStart) {
    // Dive onto west DEM entry; pre-load China detail at the skim entry
    // lon/lat so the first forward-skim frame does not remesh+recamera.
    orbit->set_dolly_limits(kDollyMin, kDollyMax);
    const float u = globe_fly_ease((t01 - 0.50f) / (kHugStart - 0.50f));
    sample_skim_path(0.f, &look_lon, &look_lat);
    float aim_yaw = china_yaw;
    float aim_pitch = china_pitch;
    look_yaw_pitch(look_lon, look_lat, &aim_yaw, &aim_pitch);
    yaw = china_yaw + (aim_yaw - china_yaw) * u;
    pitch = china_pitch + (aim_pitch - china_pitch) * u;
    const float kDiveFloor = 1.12f * R;
    const float surface = surface_r(globe, look_lon, look_lat, kSurfaceClear);
    dist = kHighDist + ((std::max)(kDiveFloor, surface) - kHighDist) * u;
  } else {
    // West→east terrain-hug over China DEM → ocean.
    const float path_u = globe_fly_ease((t01 - kHugStart) / (1.f - kHugStart));
    if (allow_forward_skim) {
      // Horizontal forward (正前方) look-at — DEM relief under a curved horizon.
      apply_forward_terrain_hug(orbit, globe, path_u, &look_lon, &look_lat,
                                &dist);
      if (session) {
        session->update_globe_detail_lod(dist, look_lon, look_lat);
      }
      return;
    }
    // Orbit path-hug (look-at-origin): free look-at skim deadlocks FlyCube
    // present_gpu. Close China DEM still reads as forward immersion; over
    // open water climb so Gerstner + coast form a readable horizon band.
    sample_skim_path(path_u, &look_lon, &look_lat);
    look_yaw_pitch(look_lon, look_lat, &yaw, &pitch);
    const float kOrbitHugFloor = 1.08f * R;
    dist = (std::max)(kOrbitHugFloor,
                      surface_r(globe, look_lon, look_lat, kSurfaceClear));
    // Keep near-surface over the coast/sea — climbing far above R reads as a
    // space globe still and loses the skim horizon.
    if (t01 >= 0.82f) {
      dist = (std::max)(1.12f * R, (std::min)(dist, 1.22f * R));
    }
    orbit->set_dolly_limits(kDollyMin, kDollyMax);
  }

  orbit->set_yaw(yaw);
  orbit->set_pitch(pitch);
  orbit->set_distance(dist);
  if (session) {
    session->update_globe_detail_lod(dist, look_lon, look_lat);
  }
}

}  // namespace plugin

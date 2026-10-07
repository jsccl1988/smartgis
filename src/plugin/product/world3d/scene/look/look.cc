// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/look/look.h"

#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scene/present/contour.h"
#include "vista/component/world/atmosphere/environment.h"

#include <cstdio>

namespace plugin {
namespace {

void apply_china_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  orbit->reset();
  orbit->apply_world_extent(content::kChinaLonLatExtent);
  orbit->set_distance(1.45f);
  orbit->set_pitch(0.52f);
  orbit->set_yaw(content::kScene3dDefaultYaw);
}

}  // namespace

const char* world3d_look_name(World3dLook look) {
  switch (look) {
    case World3dLook::kLand:
      return "land";
    case World3dLook::kOcean:
      return "ocean";
    case World3dLook::kFull:
      return "full";
    case World3dLook::kCoast:
      return "coast";
    case World3dLook::kGlobe:
      return "globe";
    case World3dLook::kLegacy:
      return "legacy";
    case World3dLook::kEastChina:
      return "east_china";
  }
  return "unknown";
}

bool parse_world3d_look(std::string_view id, World3dLook* out) {
  if (!out || id.empty()) {
    return false;
  }
  if (id == "land") {
    *out = World3dLook::kLand;
    return true;
  }
  if (id == "ocean") {
    *out = World3dLook::kOcean;
    return true;
  }
  if (id == "full") {
    *out = World3dLook::kFull;
    return true;
  }
  if (id == "coast") {
    *out = World3dLook::kCoast;
    return true;
  }
  if (id == "globe") {
    *out = World3dLook::kGlobe;
    return true;
  }
  if (id == "legacy") {
    *out = World3dLook::kLegacy;
    return true;
  }
  if (id == "east_china") {
    *out = World3dLook::kEastChina;
    return true;
  }
  return false;
}

bool apply_world3d_look(content::Scene3dPresenter* cam,
                        content::OrbitFrame* orbit,
                        World3dLook look,
                        World3dLookSeed* out) {
  if (!cam || !orbit) {
    return false;
  }
  World3dLookSeed local;
  World3dLookSeed* seed = out ? out : &local;
  *seed = World3dLookSeed{};

  switch (look) {
    case World3dLook::kLand:
      apply_china_orbit(orbit);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_sky_enabled(false);
      cam->atmosphere_session().set_fog_enabled(false);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      break;
    case World3dLook::kOcean:
      apply_china_orbit(orbit);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      break;
    case World3dLook::kFull:
      apply_china_orbit(orbit);
      orbit->set_distance(3.6f);
      orbit->set_pitch(0.36f);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      break;
    case World3dLook::kCoast: {
      orbit->reset();
      const content::Extent2 coast{118.0, 28.0, 128.0, 36.0};
      orbit->apply_world_extent(coast);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      break;
    }
    case World3dLook::kGlobe:
      world3d_china_aim_yaw_pitch(&seed->globe_china_yaw,
                                 &seed->globe_china_pitch);
      seed->globe_flythrough = true;
      orbit->reset();
      apply_world3d_globe_flythrough(orbit, 0.f, seed->globe_china_yaw,
                                    seed->globe_china_pitch,
                                    &cam->atmosphere_session().globe_pass(),
                                    &cam->atmosphere_session());
      cam->atmosphere_session().set_globe_enabled(true);
      cam->atmosphere_session().set_sat_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(false);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      break;
    case World3dLook::kLegacy:
      cam->set_look_preset(content::Scene3dLookPreset::kLegacyStereo);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_sky_enabled(false);
      cam->atmosphere_session().set_fog_enabled(false);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      apply_china_orbit(orbit);
      break;
    case World3dLook::kEastChina:
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/false);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      orbit->reset();
      {
        constexpr content::Extent2 kEastChina{112.0, 30.0, 121.0, 38.0};
        orbit->apply_world_extent(kEastChina);
      }
      orbit->set_distance(1.45f);
      orbit->set_pitch(0.52f);
      orbit->set_yaw(2.25f);
      break;
  }
  return true;
}

bool verify_world3d_look(World3dLook look, content::Scene3dPresenter* cam) {
  if (!cam) {
    return false;
  }
  const vista::atmosphere::Environment* env =
      cam->atmosphere_session().environment();
  const bool want_ocean = look == World3dLook::kOcean ||
                          look == World3dLook::kCoast ||
                          look == World3dLook::kFull ||
                          look == World3dLook::kLegacy;
  const bool want_cloud =
      look == World3dLook::kFull || look == World3dLook::kCoast ||
      look == World3dLook::kEastChina;
  const bool want_sky = look == World3dLook::kFull ||
                        look == World3dLook::kCoast ||
                        look == World3dLook::kGlobe ||
                        look == World3dLook::kEastChina;
  const bool want_fog = look == World3dLook::kCoast ||
                        look == World3dLook::kFull ||
                        look == World3dLook::kEastChina;
  if (look == World3dLook::kLand) {
    if (env && (env->ocean_enabled() || env->cloud_enabled() ||
                env->sky_enabled() || env->fog_enabled())) {
      std::fprintf(stderr, "world3d.look: land still has passes on\n");
      return false;
    }
    return true;
  }
  if (!env) {
    std::fprintf(stderr, "world3d.look: Environment missing\n");
    return false;
  }
  if (env->ocean_enabled() != want_ocean ||
      env->cloud_enabled() != want_cloud || env->sky_enabled() != want_sky ||
      env->fog_enabled() != want_fog) {
    std::fprintf(stderr,
                 "world3d.look: flag mismatch ocean=%d cloud=%d sky=%d fog=%d "
                 "(want %d/%d/%d/%d)\n",
                 env->ocean_enabled() ? 1 : 0, env->cloud_enabled() ? 1 : 0,
                 env->sky_enabled() ? 1 : 0, env->fog_enabled() ? 1 : 0,
                 want_ocean ? 1 : 0, want_cloud ? 1 : 0, want_sky ? 1 : 0,
                 want_fog ? 1 : 0);
    return false;
  }
  if (look != World3dLook::kGlobe && look != World3dLook::kEastChina &&
      env->field_store().layer_count() == 0) {
    std::fprintf(stderr, "world3d.look: FieldStore empty\n");
    return false;
  }
  if (look == World3dLook::kGlobe &&
      !cam->atmosphere_session().globe_enabled()) {
    std::fprintf(stderr, "world3d.look: globe flag off\n");
    return false;
  }
  if (look == World3dLook::kLegacy &&
      cam->look_preset() != content::Scene3dLookPreset::kLegacyStereo) {
    std::fprintf(stderr, "world3d.look: look preset not legacy\n");
    return false;
  }
  return true;
}

bool apply_world3d_east_china_face(content::Scene3dPresenter* cam,
                                   content::OrbitFrame* orbit,
                                   bool perf_bare) {
  if (!cam || !orbit) {
    return false;
  }
  if (!apply_world3d_look(cam, orbit, World3dLook::kEastChina, nullptr)) {
    return false;
  }
  if (perf_bare) {
    cam->atmosphere_session().set_sky_enabled(false);
    cam->atmosphere_session().set_cloud_enabled(false);
    cam->atmosphere_session().set_fog_enabled(false);
    cam->atmosphere_session().set_ocean_enabled(false);
  } else {
    (void)present_world3d_contour_suite(cam);
  }
  return true;
}

bool apply_world3d_true_earth_globe(content::Scene3dPresenter* cam,
                                    content::OrbitFrame* orbit,
                                    World3dLookSeed* out) {
  if (!cam || !orbit) {
    return false;
  }
  World3dLookSeed local;
  World3dLookSeed* seed = out ? out : &local;
  if (!apply_world3d_look(cam, orbit, World3dLook::kGlobe, seed)) {
    return false;
  }
  (void)present_world3d_contour_suite(cam);
  apply_world3d_globe_flythrough(
      orbit, 0.42f, seed->globe_china_yaw, seed->globe_china_pitch,
      &cam->atmosphere_session().globe_pass(), &cam->atmosphere_session());
  return true;
}

bool ensure_world3d_legacy_overlays(content::Scene3dPresenter* cam) {
  if (!cam) {
    return false;
  }
  if (!cam->ensure_legacy_overlays()) {
    std::fprintf(stderr, "world3d.look: legacy overlays failed\n");
    return false;
  }
  // Use Presenter out-of-line count — cam->gpu() in this plugin TU can land on
  // a skewed GpuPresent (stale AtmosphereSession sizeof) and report empty.
  if (cam->legacy_label_count() <= 0) {
    std::fprintf(stderr, "world3d.look: legacy labels empty\n");
    return false;
  }
  return true;
}


}  // namespace plugin

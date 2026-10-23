// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/look/look.h"

#include "base/core/log.h"
#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scene/present/contour.h"
#include "vista/component/world/atmosphere/environment.h"

namespace plugin {
namespace {

void apply_china_orbit(content::OrbitFrame* orbit) {
  if (!orbit) {
    return;
  }
  orbit->reset();
  orbit->apply_world_extent(content::kChinaLonLatExtent);
  orbit->set_dolly_limits(content::kScene3dChinaDollyMin, 12.f);
  orbit->set_distance(content::kScene3dChinaDistance);
  orbit->set_fov_y(content::kScene3dChinaFovY);
  orbit->set_pitch(content::kScene3dChinaPitch);
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
    case World3dLook::kEarthSystem:
      return "earth_system";
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
  if (id == "earth_system") {
    *out = World3dLook::kEarthSystem;
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
      // Land-only still needs china_dem + ContourSheet. Flags alone left a
      // navy clear (atmosphere-showcase=land / SCENE3D_LAND_ONLY) with no
      // mesh — same national Contour face as product Scene3D tab.
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
      cam->atmosphere_session().set_ocean_enabled(false);
      cam->atmosphere_session().set_cloud_enabled(false);
      cam->atmosphere_session().set_sky_enabled(false);
      cam->atmosphere_session().set_fog_enabled(false);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      (void)cam->atmosphere_session().apply_contour_suite_defaults();
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
      // Same ContourSheet + china_dem bind as kLand — flags alone leave a
      // translucent Local DEM plane on navy (atmosphere-showcase=full score).
      cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
      cam->atmosphere_session().set_globe_enabled(false);
      cam->atmosphere_session().set_sat_cloud_enabled(false);
      cam->atmosphere_session().set_ocean_enabled(true);
      cam->atmosphere_session().set_cloud_enabled(true);
      cam->atmosphere_session().set_sky_enabled(true);
      cam->atmosphere_session().set_fog_enabled(true);
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      (void)cam->atmosphere_session().apply_contour_suite_defaults();
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
      orbit->set_dolly_limits(content::kScene3dDollyMinLocal, 12.f);
      orbit->set_distance(content::kScene3dDefaultDistance);
      orbit->set_pitch(0.52f);
      orbit->set_yaw(2.25f);
      break;
    case World3dLook::kEarthSystem: {
      // Startup picture: high-orbit globe (space context), sky limb, satellite
      // cloud, and globe DEM for land and sea. Flat Gerstner ocean and
      // volumetric cloud are skipped while globe_on (present draws them only
      // when the globe is off — they fight the sphere). Fog, the planar China
      // contour stack, hex/ortho grids, point clouds, and model water are not
      // seeded. t=0.40 is the flythrough high-China hold (~2.2 R): China DEM
      // starts to blend, still above the surface skim.
      if (!apply_world3d_look(cam, orbit, World3dLook::kGlobe, seed)) {
        return false;
      }
      constexpr float kHighOrbitT = 0.40f;
      apply_world3d_globe_flythrough(
          orbit, kHighOrbitT, seed->globe_china_yaw, seed->globe_china_pitch,
          &cam->atmosphere_session().globe_pass(), &cam->atmosphere_session());
      content::AtmosphereSession& session = cam->atmosphere_session();
      session.set_globe_enabled(true);
      session.set_sat_cloud_enabled(true);
      session.set_sky_enabled(true);
      session.set_fog_enabled(false);
      session.set_ocean_enabled(false);
      session.set_cloud_enabled(false);
      session.set_wind_overlay_enabled(false);
      session.set_elevation_overlay(false, false);
      if (vista::atmosphere::Environment* env = session.environment()) {
        env->set_contour_enabled(false);
      }
      cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
      break;
    }
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
                        look == World3dLook::kEastChina ||
                        look == World3dLook::kEarthSystem;
  const bool want_fog = look == World3dLook::kCoast ||
                        look == World3dLook::kFull ||
                        look == World3dLook::kEastChina;
  if (look == World3dLook::kLand) {
    if (env && (env->ocean_enabled() || env->cloud_enabled() ||
                env->sky_enabled() || env->fog_enabled())) {
      LOGGING(LOG_ERROR, "world3d.look: land still has passes on");
      return false;
    }
    return true;
  }
  if (!env) {
    LOGGING(LOG_ERROR, "world3d.look: Environment missing");
    return false;
  }
  if (env->ocean_enabled() != want_ocean ||
      env->cloud_enabled() != want_cloud || env->sky_enabled() != want_sky ||
      env->fog_enabled() != want_fog) {
    LOGGING(LOG_ERROR,
            "world3d.look: flag mismatch ocean=%d cloud=%d sky=%d fog=%d "
            "(want %d/%d/%d/%d)",
            env->ocean_enabled() ? 1 : 0, env->cloud_enabled() ? 1 : 0,
            env->sky_enabled() ? 1 : 0, env->fog_enabled() ? 1 : 0,
            want_ocean ? 1 : 0, want_cloud ? 1 : 0, want_sky ? 1 : 0,
            want_fog ? 1 : 0);
    return false;
  }
  if (look != World3dLook::kGlobe && look != World3dLook::kEastChina &&
      look != World3dLook::kEarthSystem &&
      env->field_store().layer_count() == 0) {
    LOGGING(LOG_ERROR, "world3d.look: FieldStore empty");
    return false;
  }
  if ((look == World3dLook::kGlobe || look == World3dLook::kEarthSystem) &&
      !cam->atmosphere_session().globe_enabled()) {
    LOGGING(LOG_ERROR, "world3d.look: globe flag off");
    return false;
  }
  if (look == World3dLook::kEarthSystem &&
      (!cam->atmosphere_session().sat_cloud_enabled() ||
       cam->atmosphere_session().wind_overlay_enabled())) {
    LOGGING(LOG_ERROR, "world3d.look: earth_system sat=%d wind=%d",
            cam->atmosphere_session().sat_cloud_enabled() ? 1 : 0,
            cam->atmosphere_session().wind_overlay_enabled() ? 1 : 0);
    return false;
  }
  if (look == World3dLook::kLegacy &&
      cam->look_preset() != content::Scene3dLookPreset::kLegacyStereo) {
    LOGGING(LOG_ERROR, "world3d.look: look preset not legacy");
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
    LOGGING(LOG_ERROR, "world3d.look: legacy overlays failed");
    return false;
  }
  // Use Presenter out-of-line count — cam->gpu() in this plugin TU can land on
  // a skewed GpuPresent (stale AtmosphereSession sizeof) and report empty.
  if (cam->legacy_label_count() <= 0) {
    LOGGING(LOG_ERROR, "world3d.look: legacy labels empty");
    return false;
  }
  return true;
}


}  // namespace plugin

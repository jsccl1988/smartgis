// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/look/china_stack.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "vista/component/world/atmosphere/environment.h"

#include <cstdio>

namespace plugin {
namespace {

// Same framing as World3dLook::kFull so stack BMPs differ only by layer flags.
constexpr float kChinaStackDistance = 3.6f;
constexpr float kChinaStackPitch = 0.36f;

}  // namespace

const char* china_stack_stage_name(ChinaStackStage stage) {
  switch (stage) {
    case ChinaStackStage::kDem:
      return "dem";
    case ChinaStackStage::kOcean:
      return "ocean";
    case ChinaStackStage::kFog:
      return "fog";
    case ChinaStackStage::kSky:
      return "sky";
    case ChinaStackStage::kCloud:
      return "cloud";
  }
  return "unknown";
}

bool apply_china_stack_stage(content::Scene3dPresenter* cam,
                             content::OrbitFrame* orbit,
                             ChinaStackStage stage) {
  if (!cam || !orbit) {
    return false;
  }
  if (!apply_world3d_look(cam, orbit, World3dLook::kLand, nullptr)) {
    return false;
  }
  orbit->set_distance(kChinaStackDistance);
  orbit->set_pitch(kChinaStackPitch);
  const ChinaStackMask mask = china_stack_mask(stage);
  content::AtmosphereSession& session = cam->atmosphere_session();
  session.set_ocean_enabled(mask.ocean);
  session.set_fog_enabled(mask.fog);
  session.set_sky_enabled(mask.sky);
  session.set_cloud_enabled(mask.cloud);
  session.set_globe_enabled(false);
  session.set_sat_cloud_enabled(false);
  return true;
}

bool verify_china_stack_stage(content::Scene3dPresenter* cam,
                              ChinaStackStage stage) {
  if (!cam) {
    return false;
  }
  const vista::atmosphere::Environment* env =
      cam->atmosphere_session().environment();
  if (!env) {
    std::fprintf(stderr, "china-stack: Environment missing\n");
    return false;
  }
  const ChinaStackMask want = china_stack_mask(stage);
  const bool ocean = env->ocean_enabled();
  const bool fog = env->fog_enabled();
  const bool sky = env->sky_enabled();
  const bool cloud = env->cloud_enabled();
  const bool globe = cam->atmosphere_session().globe_enabled();
  const bool sat = cam->atmosphere_session().sat_cloud_enabled();
  if (ocean != want.ocean || fog != want.fog || sky != want.sky ||
      cloud != want.cloud || globe || sat) {
    std::fprintf(stderr,
                 "china-stack: %s flag mismatch ocean=%d fog=%d sky=%d "
                 "cloud=%d globe=%d sat=%d (want %d/%d/%d/%d/0/0)\n",
                 china_stack_stage_name(stage), ocean ? 1 : 0, fog ? 1 : 0,
                 sky ? 1 : 0, cloud ? 1 : 0, globe ? 1 : 0, sat ? 1 : 0,
                 want.ocean ? 1 : 0, want.fog ? 1 : 0, want.sky ? 1 : 0,
                 want.cloud ? 1 : 0);
    return false;
  }
  return true;
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/seed/mode_seed.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/atmosphere/seed/legacy_seed.h"
#include "plugin/product/world3d/scenario/atmosphere/common/progress.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "vista/component/atmosphere/environment.h"

#include <cstdio>

namespace plugin {
namespace detail {
namespace {

plugin::World3dLook look_for_mode(AtmosphereShowcaseMode mode) {
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      return plugin::World3dLook::kLand;
    case AtmosphereShowcaseMode::kOcean:
      return plugin::World3dLook::kOcean;
    case AtmosphereShowcaseMode::kFull:
      return plugin::World3dLook::kFull;
    case AtmosphereShowcaseMode::kCoast:
      return plugin::World3dLook::kCoast;
    case AtmosphereShowcaseMode::kGlobe:
      return plugin::World3dLook::kGlobe;
    case AtmosphereShowcaseMode::kLegacy:
      return plugin::World3dLook::kLegacy;
    case AtmosphereShowcaseMode::kNone:
    default:
      return plugin::World3dLook::kLand;
  }
}

}  // namespace

int seed_atmosphere_mode(HarnessShell& browser,
                         AtmosphereShowcaseMode mode,
                         content::Scene3dPresenter* cam,
                         content::OrbitFrame* orbit,
                         AtmosphereModeSeed* out) {
  if (!cam || !orbit || !out) {
    return 50;
  }
  *out = AtmosphereModeSeed{};

  atmosphere_mark("orbit-reset");
  atmosphere_mark("extent-ok");

  if (mode == AtmosphereShowcaseMode::kNone) {
    return 53;
  }

  if (mode == AtmosphereShowcaseMode::kLegacy) {
    if (const int rc = seed_atmosphere_legacy_mode(browser, cam)) {
      return rc;
    }
    atmosphere_mark("demo-ok");
    return 0;
  }

  plugin::World3dLookSeed seed;
  if (!plugin::apply_world3d_look(cam, orbit, look_for_mode(mode), &seed)) {
    return 50;
  }
  out->globe_china_yaw = seed.globe_china_yaw;
  out->globe_china_pitch = seed.globe_china_pitch;
  out->globe_flythrough = seed.globe_flythrough;
  if (mode == AtmosphereShowcaseMode::kGlobe) {
    atmosphere_mark("globe-ok");
  }
  atmosphere_mark("demo-ok");
  return 0;
}

int verify_atmosphere_mode_flags(AtmosphereShowcaseMode mode,
                                 content::Scene3dPresenter* cam) {
  if (!cam) {
    return 50;
  }
  if (!plugin::verify_world3d_look(look_for_mode(mode), cam)) {
    return 53;
  }
  atmosphere_mark("config-ok");
  const vista::atmosphere::Environment* env =
      cam->atmosphere_session().environment();
  std::fprintf(stderr, "atmosphere-showcase: ocean=%d cloud=%d layers=%zu\n",
               env && env->ocean_enabled() ? 1 : 0,
               env && env->cloud_enabled() ? 1 : 0,
               env ? env->field_store().layer_count() : 0u);
  return 0;
}

}  // namespace detail
}  // namespace plugin

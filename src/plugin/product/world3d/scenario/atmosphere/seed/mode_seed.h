// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_MODE_SEED_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_MODE_SEED_H_

#include "app/views/app/cmdline/views_launch_options.h"

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace plugin {

class HarnessShell;

namespace detail {

using AtmosphereShowcaseMode = ::app::AtmosphereShowcaseMode;

// Outputs from seeding orbit / atmosphere session for a showcase mode.
struct AtmosphereModeSeed {
  float globe_china_yaw = 0.f;
  float globe_china_pitch = 0.f;
  bool globe_flythrough = false;
};

// Seeds orbit + atmosphere flags for |mode|. Returns 0 on success, else an
// exit code (caller owns device/HWND cleanup). Writes progress marks.
int seed_atmosphere_mode(HarnessShell& browser,
                         AtmosphereShowcaseMode mode,
                         content::Scene3dPresenter* cam,
                         content::OrbitFrame* orbit,
                         AtmosphereModeSeed* out);

// Verifies Environment / globe flags match the mode contract. Returns 0 or 53.
int verify_atmosphere_mode_flags(AtmosphereShowcaseMode mode,
                                 content::Scene3dPresenter* cam);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_MODE_SEED_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SEED_MODE_SEED_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SEED_MODE_SEED_H_

#include "app/views/shell/app/cmdline/views_launch_options.h"

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Outputs from seeding orbit / atmosphere session for a showcase mode.
struct AtmosphereModeSeed {
  float globe_china_yaw = 0.f;
  float globe_china_pitch = 0.f;
  bool globe_flythrough = false;
};

// Seeds orbit + atmosphere flags for |mode|. Returns 0 on success, else an
// exit code (caller owns device/HWND cleanup). Writes progress marks.
int seed_atmosphere_mode(Browser& browser,
                         AtmosphereShowcaseMode mode,
                         content::Scene3dPresenter* cam,
                         content::OrbitFrame* orbit,
                         AtmosphereModeSeed* out);

// Verifies Environment / globe flags match the mode contract. Returns 0 or 53.
int verify_atmosphere_mode_flags(AtmosphereShowcaseMode mode,
                                 content::Scene3dPresenter* cam);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SEED_MODE_SEED_H_

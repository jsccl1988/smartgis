// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SHOWCASE_ATMOSPHERE_PRESENT_PRESENT_RUN_H_
#define APP_VIEWS_HARNESS_SHOWCASE_ATMOSPHERE_PRESENT_PRESENT_RUN_H_

#include "app/views/app/cmdline/views_launch_options.h"
#include "app/views/harness/showcase/atmosphere/session/device_session.h"
#include "app/views/harness/showcase/atmosphere/seed/mode_seed.h"

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace app {

class Browser;

namespace detail {

// Warmup presents, optional globe fly, BMP capture, timed/until-close linger.
// Returns showcase exit code (0 / 52 / 54). Does not shut down the Device
// (FlyCube teardown policy); destroys owned HWND and calls detach_maps.
int run_atmosphere_present(Browser& browser,
                           AtmosphereShowcaseMode mode,
                           const char* mode_name,
                           AtmosphereDeviceSession* session,
                           content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit,
                           const AtmosphereModeSeed& seed);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_SHOWCASE_ATMOSPHERE_PRESENT_PRESENT_RUN_H_

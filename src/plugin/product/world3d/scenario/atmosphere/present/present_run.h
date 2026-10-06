// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_PRESENT_RUN_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_PRESENT_RUN_H_

#include "app/views/app/cmdline/views_launch_options.h"
#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"
#include "plugin/product/world3d/scenario/atmosphere/seed/mode_seed.h"

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace plugin {

class HarnessShell;

namespace detail {

// Warmup presents, optional globe fly, BMP capture, timed/until-close linger.
// Returns showcase exit code (0 / 52 / 54). Does not shut down the Device
// (FlyCube teardown policy); destroys owned HWND and calls detach_maps.
int run_atmosphere_present(HarnessShell& browser,
                           AtmosphereShowcaseMode mode,
                           const char* mode_name,
                           AtmosphereDeviceSession* session,
                           content::Scene3dPresenter* cam,
                           content::OrbitFrame* orbit,
                           const AtmosphereModeSeed& seed);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_PRESENT_RUN_H_

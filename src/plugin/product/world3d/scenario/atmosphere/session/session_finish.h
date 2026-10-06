// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_SESSION_FINISH_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_SESSION_FINISH_H_

#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"

namespace plugin {

class HarnessShell;

namespace detail {

// Destroys owned present HWND, optionally shuts down Device, then detach_maps.
// |session| may be null (still detaches maps). Live GPU present paths should
// pass shutdown_device=false (FlyCube DX12 teardown heap-corrupts ExitProcess).
void finish_atmosphere_device_session(HarnessShell& browser,
                                      AtmosphereDeviceSession* session,
                                      bool shutdown_device);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_ATMOSPHERE_SESSION_FINISH_H_

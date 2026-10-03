// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SESSION_SESSION_FINISH_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SESSION_SESSION_FINISH_H_

#include "app/views/shell/harness/showcase/atmosphere/session/device_session.h"

namespace app {

class Browser;

namespace detail {

// Destroys owned present HWND, optionally shuts down Device, then detach_maps.
// |session| may be null (still detaches maps). Live GPU present paths should
// pass shutdown_device=false (FlyCube DX12 teardown heap-corrupts ExitProcess).
void finish_atmosphere_device_session(Browser& browser,
                                      AtmosphereDeviceSession* session,
                                      bool shutdown_device);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_ATMOSPHERE_SESSION_SESSION_FINISH_H_

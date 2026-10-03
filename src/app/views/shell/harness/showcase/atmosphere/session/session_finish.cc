// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/session/session_finish.h"

#include "app/views/shell/harness/common/present/rhi_present_session.h"

namespace app {
namespace detail {

void finish_atmosphere_device_session(Browser& browser,
                                      AtmosphereDeviceSession* session,
                                      bool shutdown_device) {
  RhiPresentTeardownOpts opts;
  opts.shutdown_device = shutdown_device;
  opts.destroy_hwnd = true;
  opts.detach_maps = true;
  RhiPresentSession core;
  if (session) {
    core.device = session->device;
    core.present_hwnd = session->present_hwnd;
    core.owned_present_hwnd = session->owned_present_hwnd;
    core.want_gpu = session->want_gpu;
  }
  teardown_rhi_present_session(&browser, nullptr, session ? &core : nullptr,
                               opts);
  if (session) {
    session->device = core.device;
    session->present_hwnd = core.present_hwnd;
    session->owned_present_hwnd = core.owned_present_hwnd;
  }
}

}  // namespace detail
}  // namespace app

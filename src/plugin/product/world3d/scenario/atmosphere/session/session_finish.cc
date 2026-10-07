// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/atmosphere/session/session_finish.h"

#include "plugin/product/world3d/scenario/common/host_rhi.h"

namespace plugin {
namespace detail {

void finish_atmosphere_device_session(HarnessShell& browser,
                                      AtmosphereDeviceSession* session,
                                      bool shutdown_device) {
  RhiPresentTeardownOpts opts;
  const bool borrowed = session && session->borrowed_shell;
  opts.shutdown_device = shutdown_device && !borrowed;
  opts.destroy_hwnd = !borrowed;
  // Borrowed FlyCube: detach_views after a live present AVs / ExitProcess(-1)
  // before atmosphere "pass" (visual_review #5). Leave the shell viewports up;
  // exit_after_scenario TerminateProcess tears the process down.
  opts.detach_views = !borrowed;
  RhiPresentSession core;
  if (session) {
    core.device = session->device;
    core.present_hwnd = session->present_hwnd;
    core.owned_present_hwnd = session->owned_present_hwnd;
    core.want_gpu = session->want_gpu;
    core.borrowed_shell = session->borrowed_shell;
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
}  // namespace plugin

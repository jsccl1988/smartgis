// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/session/session_finish.h"

#include "app/views/shell/harness/common/present/rhi_present_session.h"

namespace app {
namespace detail {
namespace {

RhiPresentSession to_rhi(PluginDeviceSession* session) {
  RhiPresentSession core;
  if (!session) {
    return core;
  }
  core.device = session->device;
  core.present_hwnd = session->present_hwnd;
  core.owned_present_hwnd = session->owned_present_hwnd;
  core.want_gpu = session->want_gpu;
  return core;
}

void from_rhi(PluginDeviceSession* session, const RhiPresentSession& core) {
  if (!session) {
    return;
  }
  session->device = core.device;
  session->present_hwnd = core.present_hwnd;
  session->owned_present_hwnd = core.owned_present_hwnd;
}

}  // namespace

void destroy_plugin_owned_hwnd(PluginDeviceSession* session) {
  RhiPresentSession core = to_rhi(session);
  destroy_rhi_owned_present_hwnd(&core);
  from_rhi(session, core);
}

void teardown_plugin_device_session(content::Scene3dPresenter* cam,
                                    PluginDeviceSession* session,
                                    const PluginTeardownOpts& opts) {
  RhiPresentTeardownOpts core_opts;
  core_opts.shutdown_device = opts.shutdown_device;
  core_opts.destroy_hwnd = opts.destroy_owned_hwnd;
  core_opts.detach_maps = false;
  core_opts.clear_pointcloud = opts.clear_pointcloud;
  core_opts.clear_tin = opts.clear_tin;
  core_opts.abandon_mesh = opts.abandon_mesh;
  RhiPresentSession core = to_rhi(session);
  teardown_rhi_present_session(nullptr, cam, &core, core_opts);
  from_rhi(session, core);
}

}  // namespace detail
}  // namespace app

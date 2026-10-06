// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/session/device_session.h"

#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/common/host_rhi.h"

namespace plugin {
namespace detail {
namespace {

void plugin_mark(const char* step) {
  plugin_showcase_mark(step);
}

RhiPresentSession to_rhi(PluginDeviceSession* session) {
  RhiPresentSession core;
  if (!session) {
    return core;
  }
  core.device = session->device;
  core.present_hwnd = session->present_hwnd;
  core.owned_present_hwnd = session->owned_present_hwnd;
  core.want_gpu = session->want_gpu;
  core.borrowed_shell = session->borrowed_shell;
  return core;
}

void from_rhi(PluginDeviceSession* session, const RhiPresentSession& core) {
  if (!session) {
    return;
  }
  session->device = core.device;
  session->present_hwnd = core.present_hwnd;
  session->owned_present_hwnd = core.owned_present_hwnd;
  session->want_gpu = core.want_gpu;
  session->borrowed_shell = core.borrowed_shell;
}

}  // namespace

bool resolve_plugin_want_gpu(const char* primary_gpu_env) {
  GpuEnvOpts opts;
  opts.gpu_env = primary_gpu_env;
  opts.gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  opts.gpu_env_fallback = "plugin-world3d-gpu";
  return resolve_rhi_want_gpu(opts);
}

int prepare_plugin_device_session(HarnessShell& browser,
                                  const PluginDeviceSessionOpts& opts,
                                  PluginDeviceSession* out) {
  if (!out) {
    return 50;
  }
  *out = PluginDeviceSession{};

  RhiPresentSessionOpts core_opts;
  core_opts.gpu_env = opts.gpu_env;
  core_opts.gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  core_opts.gpu_env_fallback = "plugin-world3d-gpu";
  core_opts.require_scene_hwnd = opts.require_scene_hwnd;
  core_opts.realize_scene_hwnd = true;
  core_opts.detach_flycube = false;
  core_opts.borrow_shell_scene3d = true;
  // PluginDeviceSessionOpts.borrow_shell_scene3d means select_map_tab(1).
  core_opts.select_scene_tab = opts.borrow_shell_scene3d;
  core_opts.detach_pump_ms = 200;
  core_opts.warm_swapchain = false;
  core_opts.allow_null_without_hwnd = opts.allow_null_without_hwnd;
  core_opts.present_w = kPluginShowcasePresentW;
  core_opts.present_h = kPluginShowcasePresentH;
  core_opts.create_hwnd = nullptr;
  core_opts.mark = plugin_mark;
  core_opts.marks.scene_attach = "scene-attach";

  if (!opts.borrow_shell_scene3d) {
    plugin_mark("rhi-borrow-begin");
  }

  RhiPresentSession core;
  const int rc = prepare_rhi_present_session(browser, core_opts, &core);
  from_rhi(out, core);
  return rc;
}

void destroy_plugin_owned_hwnd(PluginDeviceSession* session) {
  RhiPresentSession core = to_rhi(session);
  destroy_rhi_owned_present_hwnd(&core);
  from_rhi(session, core);
}

void teardown_plugin_device_session(content::Scene3dPresenter* cam,
                                    PluginDeviceSession* session,
                                    const PluginTeardownOpts& opts) {
  RhiPresentTeardownOpts core_opts;
  core_opts.shutdown_device =
      opts.shutdown_device && !(session && session->borrowed_shell);
  core_opts.destroy_hwnd =
      opts.destroy_owned_hwnd && !(session && session->borrowed_shell);
  core_opts.detach_maps = false;
  core_opts.clear_pointcloud = opts.clear_pointcloud;
  core_opts.clear_tin = opts.clear_tin;
  core_opts.abandon_mesh = opts.abandon_mesh;
  RhiPresentSession core = to_rhi(session);
  teardown_rhi_present_session(nullptr, cam, &core, core_opts);
  from_rhi(session, core);
}

}  // namespace detail
}  // namespace plugin

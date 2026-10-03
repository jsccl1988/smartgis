// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/session/device_session.h"

#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "app/views/shell/harness/showcase/plugin/present/present_host.h"

namespace app {
namespace detail {
namespace {

void plugin_mark(const char* step) {
  plugin_showcase_mark(step);
}

}  // namespace

bool resolve_plugin_want_gpu(const char* primary_gpu_env) {
  RhiPresentSessionOpts opts;
  opts.gpu_env = primary_gpu_env;
  opts.gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  opts.gpu_env_fallback = "SMT_PLUGIN_WORLD3D_GPU";
  return resolve_rhi_want_gpu(opts);
}

int prepare_plugin_device_session(Browser& browser,
                                  const PluginDeviceSessionOpts& opts,
                                  PluginDeviceSession* out) {
  if (!out) {
    return 50;
  }
  *out = PluginDeviceSession{};

  RhiPresentSessionOpts core_opts;
  core_opts.gpu_env = opts.gpu_env;
  core_opts.gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  core_opts.gpu_env_fallback = "SMT_PLUGIN_WORLD3D_GPU";
  core_opts.require_scene_hwnd = opts.require_scene_hwnd;
  core_opts.detach_flycube = opts.detach_flycube;
  core_opts.detach_pump_ms = 100;
  core_opts.warm_swapchain = opts.warm_swapchain;
  core_opts.allow_null_without_hwnd = opts.allow_null_without_hwnd;
  core_opts.present_w = kPluginShowcasePresentW;
  core_opts.present_h = kPluginShowcasePresentH;
  core_opts.create_hwnd = create_plugin_showcase_hwnd;
  core_opts.mark = plugin_mark;

  RhiPresentSession core;
  if (const int rc = prepare_rhi_present_session(browser, core_opts, &core)) {
    out->device = core.device;
    out->present_hwnd = core.present_hwnd;
    out->owned_present_hwnd = core.owned_present_hwnd;
    out->want_gpu = core.want_gpu;
    return rc;
  }
  out->device = core.device;
  out->present_hwnd = core.present_hwnd;
  out->owned_present_hwnd = core.owned_present_hwnd;
  out->want_gpu = core.want_gpu;
  return 0;
}

}  // namespace detail
}  // namespace app

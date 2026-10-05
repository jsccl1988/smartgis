// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/session/device_session.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/present/rhi_present_session.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
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
  RhiPresentSessionOpts opts;
  opts.gpu_env = primary_gpu_env;
  opts.gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  opts.gpu_env_fallback = "plugin-world3d-gpu";
  return resolve_rhi_want_gpu(opts);
}

int prepare_plugin_device_session(Browser& browser,
                                  const PluginDeviceSessionOpts& opts,
                                  PluginDeviceSession* out) {
  if (!out) {
    return 50;
  }
  *out = PluginDeviceSession{};

  if (!opts.borrow_shell_scene3d) {
    plugin_mark("rhi-borrow-begin");
    ui::views::DrawHost* scene = browser.scene_draw_host();
    if (content::Scene3dPresenter* cam = browser.scene3d()) {
      const int view_id = scene ? scene->view_id() : 0;
      cam->bind_contents(browser.map_session(), view_id);
    }
    if (!scene) {
      plugin_mark("hwnd-missing");
      return 50;
    }
    if (!scene->native_view()) {
      scene->realize_native();
    }
    scene->sync_native_bounds();
    if (HWND hwnd = scene->native_view()) {
      if (IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      }
    }
    if (scene->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
      plugin_mark("scene-attach");
      scene->attach();
      scene->sync_native_bounds();
    }
    scene->set_gpu_present_visible(true);
    scene->resume_present_timer();
    (void)scene->wait_ready(2500);
    const DWORD wait0 = GetTickCount();
    while (!scene->rhi_device() && (GetTickCount() - wait0) < 4000u) {
      pump_messages(50);
    }
    if (content::Scene3dPresenter* cam = browser.scene3d()) {
      cam->bind_contents(browser.map_session(), scene->view_id());
    }
    out->borrowed_shell = true;
    out->owned_present_hwnd = nullptr;
    out->present_hwnd = shell_scene3d_capture_hwnd(scene);
    out->device = static_cast<render::rhi::Device*>(scene->rhi_device());
    out->want_gpu = resolve_plugin_want_gpu(opts.gpu_env);
    if (!out->present_hwnd && !opts.allow_null_without_hwnd) {
      plugin_mark("hwnd-missing");
      return 50;
    }
    plugin_mark("shell-scene3d-borrow");
    plugin_mark("present-hwnd-ok");
    plugin_mark(out->device ? "device-init-gpu" : "device-init-null");
    return 0;
  }

  RhiPresentSessionOpts core_opts;
  core_opts.gpu_env = opts.gpu_env;
  core_opts.gpu_policy = GpuEnvPolicy::kDefaultOnUnlessZero;
  core_opts.gpu_env_fallback = "plugin-world3d-gpu";
  core_opts.require_scene_hwnd = opts.require_scene_hwnd;
  core_opts.realize_scene_hwnd = true;
  core_opts.detach_flycube = false;
  core_opts.borrow_shell_scene3d = true;
  core_opts.detach_pump_ms = 200;
  core_opts.warm_swapchain = false;
  core_opts.allow_null_without_hwnd = opts.allow_null_without_hwnd;
  core_opts.present_w = kPluginShowcasePresentW;
  core_opts.present_h = kPluginShowcasePresentH;
  core_opts.create_hwnd = nullptr;
  core_opts.mark = plugin_mark;

  RhiPresentSession core;
  if (const int rc = prepare_rhi_present_session(browser, core_opts, &core)) {
    out->device = core.device;
    out->present_hwnd = core.present_hwnd;
    out->owned_present_hwnd = core.owned_present_hwnd;
    out->want_gpu = core.want_gpu;
    out->borrowed_shell = core.borrowed_shell;
    return rc;
  }
  out->device = core.device;
  out->present_hwnd = core.present_hwnd;
  out->owned_present_hwnd = core.owned_present_hwnd;
  out->want_gpu = core.want_gpu;
  out->borrowed_shell = core.borrowed_shell;
  return 0;
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
}  // namespace app

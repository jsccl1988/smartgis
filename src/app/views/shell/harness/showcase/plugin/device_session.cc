// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/device_session.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/pump.h"
#include "app/views/shell/harness/showcase/atmosphere/host.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

#include <cstdlib>

namespace app {
namespace detail {

bool resolve_plugin_want_gpu(const char* primary_gpu_env) {
  if (primary_gpu_env) {
    if (const char* env = std::getenv(primary_gpu_env)) {
      return !(env[0] == '0' && env[1] == '\0');
    }
  }
  if (const char* env = std::getenv("SMT_PLUGIN_WORLD3D_GPU")) {
    return !(env[0] == '0' && env[1] == '\0');
  }
  return true;
}

int prepare_plugin_device_session(Browser& browser,
                                  const PluginDeviceSessionOpts& opts,
                                  PluginDeviceSession* out) {
  if (!out) {
    return 50;
  }
  *out = PluginDeviceSession{};
  out->want_gpu = resolve_plugin_want_gpu(opts.gpu_env);

  ui::views::MapViewport* scene = browser.map_scene_viewport();
  if (opts.require_scene_hwnd) {
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      plugin_showcase_mark("scene-hwnd-missing");
      return 50;
    }
  }

  if (opts.detach_flycube && scene &&
      (scene->attach_mode() ==
           ui::views::MapViewport::AttachMode::kContentMapView ||
       scene->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube)) {
    scene->detach();
    pump_messages(100);
  }

  if (out->want_gpu) {
    out->owned_present_hwnd = create_atmosphere_showcase_hwnd(
        kPluginShowcasePresentW, kPluginShowcasePresentH);
    if (!out->owned_present_hwnd) {
      plugin_showcase_mark("present-hwnd-fail");
      return 50;
    }
    out->present_hwnd = out->owned_present_hwnd;
    plugin_showcase_mark("present-hwnd-ok");
  } else if (opts.allow_null_without_hwnd) {
    plugin_showcase_mark("bmp-skip-null");
  } else {
    out->present_hwnd = scene ? scene->native_view() : nullptr;
    if (!out->present_hwnd && scene) {
      scene->realize_native();
      out->present_hwnd = scene->native_view();
    }
    if (!out->present_hwnd) {
      plugin_showcase_mark("hwnd-missing");
      return 50;
    }
  }

  out->device = render::rhi::create_device(
      out->want_gpu ? render::rhi::preferred_gpu_backend()
                    : render::rhi::Backend::kNull);
  if (!out->device) {
    plugin_showcase_mark("device-missing");
    return 51;
  }

  render::rhi::DeviceDesc desc;
  desc.native_window = out->want_gpu ? out->present_hwnd : nullptr;
  desc.width = kPluginShowcasePresentW;
  desc.height = kPluginShowcasePresentH;
  if (!out->device->initialize(desc)) {
    plugin_showcase_mark("device-init-fail");
    out->device->shutdown();
    out->device = nullptr;
    return 51;
  }

  if (opts.warm_swapchain && out->want_gpu) {
    if (render::rhi::CommandList* warm = out->device->create_command_list()) {
      render::rhi::RenderPassDesc pass;
      pass.clear_r = 0.05f;
      pass.clear_g = 0.12f;
      pass.clear_b = 0.18f;
      pass.clear_a = 1.f;
      pass.width = desc.width;
      pass.height = desc.height;
      warm->begin_render_pass(pass);
      warm->set_viewport(0, 0, static_cast<float>(desc.width),
                         static_cast<float>(desc.height), 0, 1);
      warm->end_render_pass();
      warm->close();
      out->device->execute(warm);
      out->device->destroy_command_list(warm);
      out->device->present();
    }
  }

  plugin_showcase_mark(out->want_gpu ? "device-init-gpu" : "device-init-null");
  return 0;
}

}  // namespace detail
}  // namespace app

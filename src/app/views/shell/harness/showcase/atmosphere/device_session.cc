// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/device_session.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/showcase/atmosphere/host.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/map_viewport.h"

#include <cstdio>
#include <cstdlib>

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* step) {
  write_mark(kAtmosphereShowcaseMarkLeaf, step, /*truncate=*/false);
}

}  // namespace

int prepare_atmosphere_device_session(Browser& browser,
                                      AtmosphereDeviceSession* out) {
  if (!out) {
    return 50;
  }
  *out = AtmosphereDeviceSession{};

  const bool want_gpu = []() {
    if (const char* env = std::getenv("SMT_ATMOSPHERE_SHOWCASE_GPU")) {
      return env[0] == '1' && env[1] == '\0';
    }
    return false;
  }();
  out->want_gpu = want_gpu;

  ui::views::MapViewport* scene = browser.map_scene_viewport();
  // GPU path owns a dedicated present HWND; avoid realize_native on the tab
  // child (ContentMapView attach AVs under GDI-forced showcase + DLL churn).
  if (!want_gpu) {
    if (scene && !scene->native_view()) {
      scene->realize_native();
    }
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      std::fprintf(stderr, "atmosphere-showcase: 3D viewport HWND missing\n");
      return 50;
    }
  }
  // Bind MapContents even with view_id==0 (kNone attach) so present_gpu can
  // resolve DEM / document hosts — same as init_chrome before first 3D tab.
  if (content::Scene3dPresenter* cam = browser.scene3d()) {
    const int view_id = scene ? scene->view_id() : 0;
    cam->bind_contents(browser.map_session(), view_id);
  }
  showcase_mark("scene-hwnd-ok");

  out->linger = atmosphere_showcase_linger(want_gpu);

  // Drop any ContentMapView / prior FlyCube on the tab before we own a device.
  if (scene &&
      (scene->attach_mode() ==
           ui::views::MapViewport::AttachMode::kContentMapView ||
       scene->attach_mode() == ui::views::MapViewport::AttachMode::kFlyCube)) {
    scene->detach();
    showcase_mark("detached");
  }

  if (want_gpu) {
    out->owned_present_hwnd =
        create_atmosphere_showcase_hwnd(kAtmosphereShowcaseW,
                                        kAtmosphereShowcaseH);
    if (!out->owned_present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: present HWND create failed\n");
      return 50;
    }
    out->present_hwnd = out->owned_present_hwnd;
    showcase_mark("present-hwnd-ok");
  } else {
    out->present_hwnd = scene ? scene->native_view() : nullptr;
    if (!out->present_hwnd && scene) {
      scene->realize_native();
      out->present_hwnd = scene->native_view();
    }
    if (!out->present_hwnd) {
      std::fprintf(stderr, "atmosphere-showcase: HWND gone after detach\n");
      return 50;
    }
  }
  showcase_mark("hwnd-ready");

  out->device = render::rhi::create_device(
      want_gpu ? render::rhi::preferred_gpu_backend()
               : render::rhi::Backend::kNull);
  if (!out->device) {
    std::fprintf(stderr, "atmosphere-showcase: create_device failed\n");
    showcase_mark("device-missing");
    return 51;
  }
  showcase_mark("device-created");
  out->owns_device = true;

  render::rhi::DeviceDesc desc;
  desc.native_window = want_gpu ? out->present_hwnd : nullptr;
  desc.width = kAtmosphereShowcaseW;
  desc.height = kAtmosphereShowcaseH;
  std::fprintf(stderr,
               "atmosphere-showcase: gpu=%d linger=%s present=%p %ux%u\n",
               want_gpu ? 1 : 0,
               out->linger.until_close ? "until-close"
                                       : (out->linger.ms > 0 ? "timed" : "none"),
               static_cast<void*>(out->present_hwnd), desc.width, desc.height);
  if (!out->linger.until_close && out->linger.ms > 0) {
    std::fprintf(stderr, "atmosphere-showcase: linger_ms=%lu\n",
                 static_cast<unsigned long>(out->linger.ms));
  }
  if (!out->device->initialize(desc)) {
    std::fprintf(stderr, "atmosphere-showcase: device initialize failed\n");
    out->device->shutdown();
    showcase_mark("device-missing");
    return 51;
  }
  if (want_gpu) {
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
  showcase_mark(want_gpu ? "device-init-gpu" : "device-init-null");
  showcase_mark(want_gpu ? "flycube-ok" : "null-ok");
  return 0;
}

}  // namespace detail
}  // namespace app

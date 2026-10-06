// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/common/present/rhi_present_session.h"

#include "app/views/browser/browser.h"
#include "app/views/harness/common/io/maps.h"
#include "app/views/harness/common/pump/pump.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

#include "base/process/switches.h"

#include <cstdio>
#include <cstdlib>

namespace app {
namespace detail {
namespace {

void mark_step(const RhiPresentSessionOpts& opts, const char* step) {
  if (opts.mark && step) {
    opts.mark(step);
  }
}

void log_scene3d_hwnd(HWND hwnd, const char* tag) {
  wchar_t cls[64] = {};
  wchar_t title[96] = {};
  if (hwnd && IsWindow(hwnd)) {
    GetClassNameW(hwnd, cls, 64);
    GetWindowTextW(hwnd, title, 96);
  }
  std::fprintf(stderr, "rhi-present: %s hwnd=%p class=%ls title=%ls\n",
               tag ? tag : "scene3d", static_cast<void*>(hwnd), cls, title);
}

int borrow_shell_scene3d(Browser& browser,
                         const RhiPresentSessionOpts& opts,
                         RhiPresentSession* out) {
  browser.select_map_tab(1);
  const DWORD pump_ms =
      opts.detach_pump_ms > 0 ? opts.detach_pump_ms : 200;
  pump_messages(pump_ms);

  ui::views::DrawHost* scene = browser.scene_draw_host();
  if (!scene) {
    mark_step(opts, opts.marks.scene_hwnd_missing);
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
  if (!out->present_hwnd && !opts.allow_null_without_hwnd) {
    mark_step(opts, opts.marks.hwnd_missing);
    return 50;
  }
  log_scene3d_hwnd(scene->native_view(), "shell-native");
  log_scene3d_hwnd(scene->present_hwnd(), "shell-flycube-present");
  log_scene3d_hwnd(out->present_hwnd, "shell-capture");
  mark_step(opts, "shell-scene3d-borrow");
  mark_step(opts, opts.marks.present_hwnd_ok);
  mark_step(opts, out->device ? opts.marks.device_init_gpu
                              : opts.marks.device_init_null);
  return 0;
}

void warm_swapchain_once(render::rhi::Device* device, uint32_t width,
                         uint32_t height) {
  if (!device) {
    return;
  }
  if (render::rhi::CommandList* warm = device->create_command_list()) {
    render::rhi::RenderPassDesc pass;
    pass.clear_r = 0.05f;
    pass.clear_g = 0.12f;
    pass.clear_b = 0.18f;
    pass.clear_a = 1.f;
    pass.width = width;
    pass.height = height;
    warm->begin_render_pass(pass);
    warm->set_viewport(0, 0, static_cast<float>(width),
                       static_cast<float>(height), 0, 1);
    warm->end_render_pass();
    warm->close();
    device->execute(warm);
    device->destroy_command_list(warm);
    device->present();
  }
}

}  // namespace

bool resolve_rhi_want_gpu(const RhiPresentSessionOpts& opts) {
  if (opts.gpu_policy == GpuEnvPolicy::kDefaultOffRequireOne) {
    if (opts.gpu_env) {
      if (const char* env = base::switch_cstr(opts.gpu_env)) {
        return env[0] == '1' && env[1] == '\0';
      }
    }
    return false;
  }
  // kDefaultOnUnlessZero
  if (opts.gpu_env) {
    if (const char* env = base::switch_cstr(opts.gpu_env)) {
      return !(env[0] == '0' && env[1] == '\0');
    }
  }
  if (opts.gpu_env_fallback) {
    if (const char* env = base::switch_cstr(opts.gpu_env_fallback)) {
      return !(env[0] == '0' && env[1] == '\0');
    }
  }
  return true;
}

HWND shell_scene3d_capture_hwnd(ui::views::DrawHost* scene) {
  if (!scene) {
    return nullptr;
  }
  HWND hwnd = scene->present_hwnd();
  if (hwnd && IsWindow(hwnd)) {
    return hwnd;
  }
  hwnd = scene->input_hwnd();
  if (hwnd && IsWindow(hwnd)) {
    return hwnd;
  }
  hwnd = scene->native_view();
  if (hwnd && IsWindow(hwnd)) {
    return hwnd;
  }
  return nullptr;
}

bool present_shell_scene3d_frame(ui::views::DrawHost* scene, DWORD wait_ms) {
  if (!scene) {
    return false;
  }
  scene->set_gpu_present_visible(true);
  scene->resume_present_timer();
  const uint32_t before = scene->frame_presented();
  scene->request_frame();
  const DWORD t0 = GetTickCount();
  const DWORD budget = wait_ms > 0 ? wait_ms : 2000;
  while (scene->frame_presented() <= before) {
    if ((GetTickCount() - t0) >= budget) {
      break;
    }
    scene->request_frame();
    pump_messages(16);
  }
  // HWND-only success hid Fps 0 navy clears (present_gpu never advanced).
  return scene->frame_presented() > before && scene->last_gpu_present_ok();
}

int prepare_rhi_present_session(Browser& browser,
                                const RhiPresentSessionOpts& opts,
                                RhiPresentSession* out) {
  if (!out) {
    return 50;
  }
  *out = RhiPresentSession{};
  out->want_gpu = resolve_rhi_want_gpu(opts);

  if (opts.borrow_shell_scene3d) {
    return borrow_shell_scene3d(browser, opts, out);
  }

  ui::views::DrawHost* scene = browser.scene_draw_host();
  if (opts.require_scene_hwnd) {
    if (opts.realize_scene_hwnd && scene && !scene->native_view()) {
      scene->realize_native();
    }
    if (!scene || !scene->native_view() || !IsWindow(scene->native_view())) {
      mark_step(opts, opts.marks.scene_hwnd_missing);
      return 50;
    }
  }

  if (opts.detach_flycube && scene &&
      (scene->attach_mode() ==
           ui::views::DrawHost::AttachMode::kContentMapView ||
       scene->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent)) {
    scene->detach();
    mark_step(opts, opts.marks.detached);
    if (opts.detach_pump_ms > 0) {
      pump_messages(opts.detach_pump_ms);
    }
  }

  if (out->want_gpu) {
    if (!opts.create_hwnd) {
      mark_step(opts, opts.marks.present_hwnd_fail);
      return 50;
    }
    out->owned_present_hwnd =
        opts.create_hwnd(opts.present_w, opts.present_h);
    if (!out->owned_present_hwnd) {
      mark_step(opts, opts.marks.present_hwnd_fail);
      return 50;
    }
    out->present_hwnd = out->owned_present_hwnd;
    mark_step(opts, opts.marks.present_hwnd_ok);
  } else if (opts.allow_null_without_hwnd) {
    mark_step(opts, opts.marks.null_without_hwnd);
  } else {
    out->present_hwnd = scene ? scene->native_view() : nullptr;
    if (!out->present_hwnd && scene) {
      scene->realize_native();
      out->present_hwnd = scene->native_view();
    }
    if (!out->present_hwnd) {
      mark_step(opts, opts.marks.hwnd_missing);
      return 50;
    }
  }

  const bool scenic_host = content::prefer_scene3d_scenic();
  out->device = render::rhi::create_device(
      (out->want_gpu && !scenic_host) ? render::rhi::preferred_gpu_backend()
                                      : render::rhi::Backend::kNull);
  if (!out->device) {
    mark_step(opts, opts.marks.device_missing);
    return 51;
  }

  render::rhi::DeviceDesc desc;
  desc.native_window = out->want_gpu ? out->present_hwnd : nullptr;
  desc.width = opts.present_w;
  desc.height = opts.present_h;
  if (!out->device->initialize(desc)) {
    mark_step(opts, opts.marks.device_init_fail);
    // Shutdown only — never delete FlyCube Device* (heap corruption risk).
    out->device->shutdown();
    out->device = nullptr;
    return 51;
  }

  if (opts.warm_swapchain && out->want_gpu && !scenic_host) {
    warm_swapchain_once(out->device, opts.present_w, opts.present_h);
  }

  mark_step(opts, out->want_gpu ? opts.marks.device_init_gpu
                                : opts.marks.device_init_null);
  return 0;
}

void destroy_rhi_owned_present_hwnd(RhiPresentSession* session) {
  if (!session || !session->owned_present_hwnd) {
    return;
  }
  const HWND owned = session->owned_present_hwnd;
  DestroyWindow(owned);
  session->owned_present_hwnd = nullptr;
  if (session->present_hwnd == owned) {
    session->present_hwnd = nullptr;
  }
}

void teardown_rhi_present_session(Browser* browser,
                                  content::Scene3dPresenter* cam,
                                  RhiPresentSession* session,
                                  const RhiPresentTeardownOpts& opts) {
  if (cam) {
    if (opts.clear_pointcloud) {
      cam->clear_overlay_pointcloud();
    }
    if (opts.clear_tin) {
      cam->clear_overlay_tin_mesh();
    }
    if (opts.abandon_mesh) {
      cam->abandon_mesh();
    }
  }
  if (session) {
    if (session->borrowed_shell) {
      session->owned_present_hwnd = nullptr;
    } else {
      if (opts.shutdown_device && session->device) {
        session->device->shutdown();
        // Intentionally leak Device* after shutdown — FlyCube operator delete
        // after a live DX12 session can corrupt heaps.
        session->device = nullptr;
      }
      if (opts.destroy_hwnd) {
        destroy_rhi_owned_present_hwnd(session);
      }
    }
  }
  if (opts.detach_maps && browser) {
    detach_maps(*browser);
  }
}

}  // namespace detail
}  // namespace app

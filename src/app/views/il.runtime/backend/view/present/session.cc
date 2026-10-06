// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/present/session.h"

#include <imm.h>

#include "app/views/il.runtime/backend/view/host/capture_host.h"

#pragma comment(lib, "imm32.lib")
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "plugin/runtime/host/capability/shell.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

#include <cstdio>

namespace app {
namespace detail {
namespace {

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

void destroy_owned_present_hwnd(HWND* owned, HWND* alias) {
  if (!owned || !*owned) {
    return;
  }
  const HWND hwnd = *owned;
  if (alias && *alias == hwnd) {
    *alias = nullptr;
  }
  destroy_present_hwnd(owned);
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

// Attach DrawHost Role::kScene3d and wait for its RHI Device. Does not
// CreateWindow. |select_scene_tab| is independent of the borrow itself.
int borrow_shell_scene3d(plugin::HarnessShell& browser,
                         const RhiPresentSessionOpts& opts,
                         RhiPresentSession* out) {
  if (!out) {
    return 50;
  }
  if (opts.select_scene_tab) {
    browser.select_map_tab(1);
    const DWORD pump_ms =
        opts.detach_pump_ms > 0 ? opts.detach_pump_ms : 200;
    browser.pump(pump_ms);
  }

  ui::views::DrawHost* scene = browser.scene_draw_host();
  if (content::Scene3dPresenter* cam = browser.scene3d()) {
    const int view_id = scene ? scene->view_id() : 0;
    cam->bind_contents(browser.map_contents(), view_id);
  }
  if (!scene) {
    mark_step(opts.mark, opts.marks.scene_hwnd_missing);
    mark_step(opts.mark, opts.marks.hwnd_missing);
    return 50;
  }
  if (!scene->native_view()) {
    scene->realize_native();
    mark_step(opts.mark, opts.marks.realize_native);
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
    mark_step(opts.mark, opts.marks.scene_attach);
    scene->attach();
    scene->sync_native_bounds();
  }
  scene->set_gpu_present_visible(true);
  scene->resume_present_timer();
  (void)scene->wait_ready(2500);
  const DWORD wait0 = GetTickCount();
  while (!scene->rhi_device() && (GetTickCount() - wait0) < 4000u) {
    browser.pump(50);
  }

  if (content::Scene3dPresenter* cam = browser.scene3d()) {
    cam->bind_contents(browser.map_contents(), scene->view_id());
  }

  out->borrowed_shell = true;
  out->owned_present_hwnd = nullptr;
  out->present_hwnd = shell_scene3d_capture_hwnd(scene);
  out->device = static_cast<render::rhi::Device*>(scene->rhi_device());
  if (!out->present_hwnd && !opts.allow_null_without_hwnd) {
    mark_step(opts.mark, opts.marks.hwnd_missing);
    return 50;
  }
  log_scene3d_hwnd(scene->native_view(), "shell-native");
  log_scene3d_hwnd(scene->present_hwnd(), "shell-flycube-present");
  log_scene3d_hwnd(out->present_hwnd, "shell-capture");
  mark_step(opts.mark, opts.marks.borrow_ok);
  mark_step(opts.mark, opts.marks.present_hwnd_ok);
  mark_step(opts.mark, out->device ? opts.marks.device_init_gpu
                              : opts.marks.device_init_null);
  return 0;
}

// Create + initialize an owned RHI Device for |out->present_hwnd|.
// Failure shuts the device down but never operator-deletes FlyCube.
int init_owned_rhi_device(const RhiPresentSessionOpts& opts,
                          RhiPresentSession* out) {
  if (!out) {
    return 50;
  }
  const bool scenic_host = content::prefer_scene3d_scenic();
  out->device = render::rhi::create_device(
      (out->want_gpu && !scenic_host) ? render::rhi::preferred_gpu_backend()
                                      : render::rhi::Backend::kNull);
  if (!out->device) {
    mark_step(opts.mark, opts.marks.device_missing);
    destroy_owned_present_hwnd(&out->owned_present_hwnd, &out->present_hwnd);
    return 51;
  }

  render::rhi::DeviceDesc desc;
  desc.native_window = out->want_gpu ? out->present_hwnd : nullptr;
  desc.width = opts.present_w;
  desc.height = opts.present_h;
  if (!out->device->initialize(desc)) {
    mark_step(opts.mark, opts.marks.device_init_fail);
    // Shutdown only — never delete FlyCube Device* (heap corruption risk).
    out->device->shutdown();
    out->device = nullptr;
    destroy_owned_present_hwnd(&out->owned_present_hwnd, &out->present_hwnd);
    return 51;
  }

  if (opts.warm_swapchain && out->want_gpu && !scenic_host) {
    warm_swapchain_once(out->device, opts.present_w, opts.present_h);
  }

  mark_step(opts.mark, out->want_gpu ? opts.marks.device_init_gpu
                                : opts.marks.device_init_null);
  return 0;
}

}  // namespace

int prepare_rhi_present_session(plugin::HarnessShell& browser,
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
      mark_step(opts.mark, opts.marks.scene_hwnd_missing);
      return 50;
    }
  }

  if (opts.detach_flycube && scene &&
      (scene->attach_mode() ==
           ui::views::DrawHost::AttachMode::kContentMapView ||
       scene->attach_mode() == ui::views::DrawHost::AttachMode::kGpuPresent)) {
    scene->detach();
    mark_step(opts.mark, opts.marks.detached);
    if (opts.detach_pump_ms > 0) {
      pump_messages(opts.detach_pump_ms);
    }
  }

  if (out->want_gpu) {
    if (opts.create_hwnd) {
      out->owned_present_hwnd =
          opts.create_hwnd(opts.present_w, opts.present_h);
    } else if (opts.hwnd.class_name && opts.hwnd.window_title) {
      PresentHwndOpts hwnd = opts.hwnd;
      hwnd.width_px = opts.present_w;
      hwnd.height_px = opts.present_h;
      out->owned_present_hwnd = create_present_hwnd(hwnd);
    }
    if (!out->owned_present_hwnd) {
      mark_step(opts.mark, opts.marks.present_hwnd_fail);
      return 50;
    }
    out->present_hwnd = out->owned_present_hwnd;
    mark_step(opts.mark, opts.marks.present_hwnd_ok);
  } else if (opts.allow_null_without_hwnd) {
    mark_step(opts.mark, opts.marks.null_without_hwnd);
  } else {
    out->present_hwnd = scene ? scene->native_view() : nullptr;
    if (!out->present_hwnd && scene) {
      scene->realize_native();
      out->present_hwnd = scene->native_view();
    }
    if (!out->present_hwnd) {
      mark_step(opts.mark, opts.marks.hwnd_missing);
      return 50;
    }
  }

  return init_owned_rhi_device(opts, out);
}

void destroy_rhi_owned_present_hwnd(RhiPresentSession* session) {
  if (!session) {
    return;
  }
  destroy_owned_present_hwnd(&session->owned_present_hwnd,
                             &session->present_hwnd);
}

void teardown_rhi_present_session(plugin::HarnessShell* browser,
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
      // Pointers belong to DrawHost. Drop them so a later teardown cannot
      // shutdown or DestroyWindow the live shell surface.
      session->owned_present_hwnd = nullptr;
      session->present_hwnd = nullptr;
      session->device = nullptr;
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
    browser->detach_maps();
  }
}

namespace {

LRESULT CALLBACK present_wnd_proc(HWND hwnd, UINT msg, WPARAM wp,
                                  LPARAM lp) {
  // Swallow C++ EH from third-party window hooks (TSF) so CreateWindow /
  // DispatchMessage cannot escalate to STATUS_FATAL_USER_CALLBACK_EXCEPTION.
  try {
    switch (msg) {
      case WM_ERASEBKGND:
        return 1;
      case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
      }
      default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
  } catch (...) {
    return 0;
  }
}

}  // namespace

HWND create_present_hwnd(const PresentHwndOpts& opts) {
  if (!opts.class_name || !opts.window_title || opts.width_px < 8 ||
      opts.height_px < 8) {
    return nullptr;
  }
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = present_wnd_proc;
  wc.hInstance = inst;
  wc.lpszClassName = opts.class_name;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassExW(&wc);

  RECT wr = {0, 0, static_cast<LONG>(opts.width_px),
             static_cast<LONG>(opts.height_px)};
  AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
  // WS_EX_NOACTIVATE + SHOWNOACTIVATE: avoid foreground focus that re-arms
  // TSF/IME callbacks during harness CaptureWindow creation.
  HWND hwnd = CreateWindowExW(
      WS_EX_NOACTIVATE, opts.class_name, opts.window_title,
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, inst, nullptr);
  if (!hwnd) {
    return nullptr;
  }
  ImmAssociateContext(hwnd, nullptr);
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd);
  return hwnd;
}

void destroy_present_hwnd(HWND* hwnd) {
  if (!hwnd || !*hwnd) {
    return;
  }
  DestroyWindow(*hwnd);
  *hwnd = nullptr;
}

}  // namespace detail
}  // namespace app

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/host/capture_host.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/horizon/atom/pump.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "render/rhi/rhi.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {
namespace {

template <typename Fn>
void for_each_draw_host(Browser& browser, Fn&& fn) {
  fn(browser.draw_host());
  fn(browser.data_draw_host());
  fn(browser.scene_draw_host());
}

}  // namespace

void detach_views(Browser& browser) {
  // Timers first: detach alone leaves queued WM_TIMER presents racing
  // ContentMapView / session teardown under Debug CRT ExitProcess.
  // Do NOT abandon_mesh here — under FlyCube Scene3D that remaps heap and
  // yields harness_rc 0xFFFFFFFF (browse.3d) after in-proc IL marks land.
  stop_present_timers(browser);
  for_each_draw_host(browser, [](ui::views::DrawHost* pane) {
    if (pane) {
      pane->detach();
    }
  });
}

void stop_present_timers(Browser& browser) {
  // pause_present KillTimer + ignore queued ticks (no PeekMessage: that
  // processes sent Display messages and deadlocks UI↔Display).
  for_each_draw_host(browser, [](ui::views::DrawHost* pane) {
    if (pane) {
      pane->pause_present();
    }
  });
}

void resume_present_timers(Browser& browser) {
  for_each_draw_host(browser, [](ui::views::DrawHost* pane) {
    if (!pane) {
      return;
    }
    pane->set_gpu_present_visible(true);
    pane->resume_present_timer();
    pane->invalidate_native();
  });
}

void kick_draw_host_paint(ui::views::DrawHost* pane) {
  if (!pane) {
    return;
  }
  if (HWND hwnd = pane->native_view()) {
    if (IsWindow(hwnd)) {
      InvalidateRect(hwnd, nullptr, FALSE);
      UpdateWindow(hwnd);
    }
  }
  pane->invalidate_native();
  pane->sync_identity_frame();
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

bool present_scene3d_gpu(content::Scene3dPresenter* cam,
                         render::rhi::Device* device,
                         uint32_t width_px,
                         uint32_t height_px) {
  if (!cam || !device) {
    return false;
  }
  return cam->present_gpu(device, width_px, height_px);
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

void finish_scene3d(Browser& browser, bool borrowed_shell) {
  if (!borrowed_shell) {
    detach_views(browser);
    return;
  }
  stop_present_timers(browser);
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    scene->pause_present();
  }
}

}  // namespace detail
}  // namespace app

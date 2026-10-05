// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/io/maps.h"

#include "app/views/shell/browser/browser.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

void detach_maps(Browser& browser) {
  // Timers first: detach alone leaves queued WM_TIMER presents racing
  // ContentMapView / session teardown under Debug CRT ExitProcess.
  // Do NOT abandon_mesh here — under FlyCube Scene3D that remaps heap and
  // yields showcase_rc 0xFFFFFFFF (browse.3d) after in-proc IL marks land.
  stop_map_present_timers(browser);
  if (ui::views::DrawHost* m = browser.draw_host()) {
    m->detach();
  }
  if (ui::views::DrawHost* m = browser.data_draw_host()) {
    m->detach();
  }
  if (ui::views::DrawHost* m = browser.scene_draw_host()) {
    m->detach();
  }
}

void finish_scene3d_showcase(Browser& browser, bool borrowed_shell) {
  if (!borrowed_shell) {
    detach_maps(browser);
    return;
  }
  stop_map_present_timers(browser);
  if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
    scene->pause_present();
  }
}

void stop_map_present_timers(Browser& browser) {
  // Must match DrawHost::kPresentTimerId (1). KillTimer alone leaves any
  // already-queued WM_TIMER in the message queue — drain those so browse /
  // navigate stress does not race a late present tick with synthetic input.
  constexpr UINT_PTR k_present_timer_id = 1;
  auto stop = [](ui::views::DrawHost* pane) {
    if (!pane) {
      return;
    }
    HWND hwnd = pane->native_view();
    if (!hwnd || !IsWindow(hwnd)) {
      return;
    }
    KillTimer(hwnd, k_present_timer_id);
    MSG msg;
    while (PeekMessageW(&msg, hwnd, WM_TIMER, WM_TIMER, PM_REMOVE)) {
      if (msg.wParam != k_present_timer_id) {
        // Preserve unrelated timers on the same HWND.
        PostMessageW(hwnd, msg.message, msg.wParam, msg.lParam);
      }
    }
  };
  stop(browser.draw_host());
  stop(browser.data_draw_host());
  stop(browser.scene_draw_host());
}

void resume_map_present_timers(Browser& browser) {
  auto resume = [](ui::views::DrawHost* pane) {
    if (!pane) {
      return;
    }
    pane->set_gpu_present_visible(true);
    pane->resume_present_timer();
    pane->invalidate_native();
  };
  resume(browser.draw_host());
  resume(browser.data_draw_host());
  resume(browser.scene_draw_host());
}

}  // namespace detail
}  // namespace app

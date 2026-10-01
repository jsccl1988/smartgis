// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/maps.h"

#include "app/views/shell/browser/browser.h"
#include "ui/views/map/map_viewport.h"

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
  if (ui::views::MapViewport* m = browser.map_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_data_viewport()) {
    m->detach();
  }
  if (ui::views::MapViewport* m = browser.map_scene_viewport()) {
    m->detach();
  }
}

void stop_map_present_timers(Browser& browser) {
  // Must match MapViewport::kPresentTimerId (1). KillTimer alone leaves any
  // already-queued WM_TIMER in the message queue — drain those so browse /
  // navigate stress does not race a late present tick with synthetic input.
  constexpr UINT_PTR k_present_timer_id = 1;
  auto stop = [](ui::views::MapViewport* pane) {
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
  stop(browser.map_viewport());
  stop(browser.map_data_viewport());
  stop(browser.map_scene_viewport());
}

}  // namespace detail
}  // namespace app

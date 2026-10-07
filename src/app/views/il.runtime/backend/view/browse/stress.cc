// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/browse/stress.h"

#include <cstdio>

#include "app/views/browser/browser.h"
#include "content/browser/present/host/blit_frame_cache.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "content/public/tool_session.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace {

// One left-button press, drag, and release in viewport pixels.
struct DragStroke {
  int x = 0;
  int y = 0;
  int dx = 0;
  int dy = 0;
};

// Holds skip-map-context-menu for a stress burst so every return path clears it.
struct SkipMapContextMenu {
  SkipMapContextMenu() {
    SetEnvironmentVariableA("skip-map-context-menu", "1");
  }
  ~SkipMapContextMenu() {
    SetEnvironmentVariableA("skip-map-context-menu", nullptr);
  }
  SkipMapContextMenu(const SkipMapContextMenu&) = delete;
  SkipMapContextMenu& operator=(const SkipMapContextMenu&) = delete;
};

// SEH must not share a frame with C++ objects that need unwind.
void update_map_hwnd_seh(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  __try {
    UpdateWindow(hwnd);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

bool play_drag(content::ToolSession& host, DragStroke stroke) {
  content::InputEvent down{};
  down.kind = content::InputEvent::Kind::kLDown;
  down.x_px = stroke.x;
  down.y_px = stroke.y;
  content::InputEvent move = down;
  move.kind = content::InputEvent::Kind::kMouseMove;
  move.x_px += stroke.dx;
  move.y_px += stroke.dy;
  content::InputEvent up = move;
  up.kind = content::InputEvent::Kind::kLUp;
  return host.dispatch_input(down) && host.dispatch_input(move) &&
         host.dispatch_input(up);
}

bool play_wheel(content::ToolSession& host, int x, int y, int wheel) {
  content::InputEvent event{};
  event.kind = content::InputEvent::Kind::kWheel;
  event.x_px = x;
  event.y_px = y;
  event.wheel = wheel;
  return host.dispatch_input(event);
}

// True when the host consumed RMB. view.pan must leave it for the shell menu.
bool rmb_consumed(content::ToolSession& host, int x, int y) {
  content::InputEvent down{};
  down.kind = content::InputEvent::Kind::kRDown;
  down.x_px = x;
  down.y_px = y;
  content::InputEvent up = down;
  up.kind = content::InputEvent::Kind::kRUp;
  return host.dispatch_input(down) || host.dispatch_input(up);
}

// Do not invalidate_frame_cache here — a full china rebuild per stroke drops
// PrintWindow/BitBlt below motion_gate frame counts.
void paint_map_client(Browser& browser) {
  ui::views::DrawHost* pane = browser.draw_host();
  if (!pane) {
    return;
  }
  pane->invalidate_native();
  update_map_hwnd_seh(pane->native_view());
}

}  // namespace

namespace detail {

bool browse_stress(Browser& browser, const wchar_t* leaf, int count) {
  content::ToolSession* host = browser.edit_tool_session();
  if (!host) {
    write_mark(leaf, "browse-stress-no-host", false);
    return false;
  }
  write_mark(leaf, "browse-stress-begin", false);
  // Do not KillTimer for the whole burst: ContentMapView + FORCE_GDI needs
  // WM_PAINT so HWND BitBlt / motion_gate see pan. Do not PeekMessage the
  // full UI queue (re-entrant AV); paint via UpdateWindow on the map HWND
  // after each stroke. Drop leftover StretchBlt pan preview so FORCE_GDI
  // Map2d paint is the HWND source of truth.
  SkipMapContextMenu skip_menu;
  if (browser.blit()) {
    browser.blit()->end_preview();
  }
  const int n = count > 0 ? count : 24;
  for (int i = 0; i < n; ++i) {
    if ((i % 8) == 0) {
      char step[32];
      std::snprintf(step, sizeof(step), "browse-stress-%d", i);
      write_mark(leaf, step, false);
    }
    // Large alternating pans so the motion_gate center crop sees distinct
    // frames (18px deltas were too small vs the 96x54 crop).
    const int dir = (i & 1) ? -1 : 1;
    const DragStroke stroke{120 + (i % 5) * 10, 80 + (i % 7) * 8,
                            dir * (64 + (i % 4) * 12),
                            dir * (40 + (i % 3) * 10)};
    if (!play_drag(*host, stroke)) {
      write_mark(leaf, "browse-stress-pan-fail", false);
      return false;
    }
    if (!play_wheel(*host, stroke.x + stroke.dx, stroke.y + stroke.dy,
                    (i & 1) ? 120 : -120)) {
      write_mark(leaf, "browse-stress-wheel-fail", false);
      return false;
    }
    paint_map_client(browser);
    ::Sleep(35);
  }
  ::Sleep(50);
  if (rmb_consumed(*host, 50, 50)) {
    write_mark(leaf, "browse-stress-rmb-swallowed", false);
    return false;
  }
  write_mark(leaf, "browse-stress-end", false);
  return true;
}

}  // namespace detail
}  // namespace app

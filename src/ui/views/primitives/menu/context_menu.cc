// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/menu/context_menu.h"

#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"

namespace ui {
namespace views {

void show_context_menu(HWND owner, Point screen,
                       const std::vector<MenuItem>& items) {
  if (!owner || items.empty()) {
    return;
  }
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }
  std::vector<std::function<void()>> invokes;
  invokes.reserve(items.size());
  UINT id = 1;
  for (const MenuItem& item : items) {
    if (item.separator) {
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
      continue;
    }
    UINT flags = MF_STRING;
    if (!item.enabled) {
      flags |= MF_GRAYED;
    }
    const std::wstring w = utf8_to_wide(item.label);
    AppendMenuW(menu, flags, id, w.c_str());
    invokes.push_back(item.invoke);
    ++id;
  }
  // Keep the popup inside the monitor work area (avoids clipped / off-screen
  // menus on multi-monitor and high-DPI setups).
  int x = screen.x;
  int y = screen.y;
  clamp_rect_to_work_area(&x, &y, 1, 1, owner);
  // Custom WS_POPUP frames are often not foreground when the click lands on a
  // Skia child; without this TrackPopupMenu returns 0 immediately and layer /
  // catalog context actions appear dead.
  SetForegroundWindow(owner);
  const UINT cmd = TrackPopupMenu(
      menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_HORIZONTAL | TPM_VERTICAL, x,
      y, 0, owner, nullptr);
  // Required so the menu can dismiss cleanly when the user clicks away
  // (Microsoft guidance for TrackPopupMenu on non-foreground hosts).
  PostMessageW(owner, WM_NULL, 0, 0);
  DestroyMenu(menu);
  if (cmd >= 1 && static_cast<size_t>(cmd) <= invokes.size()) {
    auto& fn = invokes[static_cast<size_t>(cmd) - 1];
    if (fn) {
      fn();
    }
  }
}

}  // namespace views
}  // namespace ui

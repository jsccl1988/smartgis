// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/context_menu.h"

#include "ui/views/theme.h"

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
  const UINT cmd =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y,
                     0, owner, nullptr);
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

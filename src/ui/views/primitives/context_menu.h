// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_CONTEXT_MENU_H_
#define UI_VIEWS_PRIMITIVES_CONTEXT_MENU_H_

#include <functional>
#include <string>
#include <vector>

#include <windows.h>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

// One row in a native popup. |separator| ignores |label| / |invoke|.
struct MenuItem {
  std::string label;
  std::function<void()> invoke;
  bool enabled = true;
  bool separator = false;
};

// Shows a Win32 TrackPopupMenu at |screen| (screen pixels) owned by |owner|.
void show_context_menu(HWND owner, Point screen, const std::vector<MenuItem>& items);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_CONTEXT_MENU_H_

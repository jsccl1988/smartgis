// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_MENU_COMPOSER_H_
#define APP_VIEWS_UI_HORIZON_MENU_COMPOSER_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {

class BrowserView;

// Top MenuBar rebuild + deferred map context menu.
class MenuComposer {
 public:
  explicit MenuComposer(BrowserView* host);
  ~MenuComposer() = default;

  MenuComposer(const MenuComposer&) = delete;
  MenuComposer& operator=(const MenuComposer&) = delete;

  void schedule_menu_rebuild();
  void rebuild_menus();
  void on_map_right_click(HWND map_hwnd, int view_x, int view_y);
  void show_pending_map_context_menu();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_MENU_COMPOSER_H_

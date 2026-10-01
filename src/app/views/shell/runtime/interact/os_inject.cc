// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/interact/os_inject.h"

namespace app {
namespace detail {

LPARAM client_lparam(int x, int y) {
  return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
}

bool post_mouse(HWND hwnd, UINT down, UINT up, int x, int y) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  const LPARAM lp = client_lparam(x, y);
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, lp);
  PostMessageW(hwnd, down, (down == WM_RBUTTONDOWN) ? MK_RBUTTON : MK_LBUTTON,
               lp);
  PostMessageW(hwnd, up, 0, lp);
  return true;
}

bool post_drag(HWND hwnd, int x0, int y0, int x1, int y1) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, client_lparam(x0, y0));
  PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, client_lparam(x0, y0));
  PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, client_lparam(x1, y1));
  PostMessageW(hwnd, WM_LBUTTONUP, 0, client_lparam(x1, y1));
  return true;
}

bool post_wheel(HWND hwnd, int x, int y, int delta) {
  if (!hwnd || !IsWindow(hwnd)) {
    return false;
  }
  POINT pt = {x, y};
  ClientToScreen(hwnd, &pt);
  PostMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<short>(delta)),
               MAKELPARAM(static_cast<WORD>(pt.x), static_cast<WORD>(pt.y)));
  return true;
}

bool post_path(HWND hwnd, const std::vector<Point>& pts) {
  if (!hwnd || pts.size() < 2) {
    return false;
  }
  PostMessageW(hwnd, WM_MOUSEMOVE, 0, client_lparam(pts[0].x, pts[0].y));
  PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               client_lparam(pts[0].x, pts[0].y));
  for (size_t i = 1; i < pts.size(); ++i) {
    PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
                 client_lparam(pts[i].x, pts[i].y));
  }
  const Point& last = pts.back();
  PostMessageW(hwnd, WM_LBUTTONUP, 0, client_lparam(last.x, last.y));
  return true;
}

WORD vk_from_name(const std::string& name) {
  if (name == "ESCAPE" || name == "Esc") {
    return VK_ESCAPE;
  }
  if (name == "RETURN" || name == "ENTER") {
    return VK_RETURN;
  }
  if (name == "TAB") {
    return VK_TAB;
  }
  if (name == "SPACE") {
    return VK_SPACE;
  }
  if (name == "CTRL" || name == "CONTROL") {
    return VK_CONTROL;
  }
  if (name == "SHIFT") {
    return VK_SHIFT;
  }
  if (name == "ALT" || name == "MENU") {
    return VK_MENU;
  }
  if (name.size() == 1) {
    const char c = name[0];
    if (c >= 'a' && c <= 'z') {
      return static_cast<WORD>(c - 'a' + 'A');
    }
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      return static_cast<WORD>(c);
    }
  }
  return 0;
}

}  // namespace detail
}  // namespace app

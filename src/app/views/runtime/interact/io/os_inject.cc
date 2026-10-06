// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/io/os_inject.h"

#include <string_view>
#include <utility>

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
  using namespace base::tuple::literals;
  if (!hwnd || pts.size() < 2) {
    return false;
  }
  PostMessageW(hwnd, WM_MOUSEMOVE, 0,
               client_lparam(pts[0]["x"_t], pts[0]["y"_t]));
  PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               client_lparam(pts[0]["x"_t], pts[0]["y"_t]));
  for (size_t i = 1; i < pts.size(); ++i) {
    PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
                 client_lparam(pts[i]["x"_t], pts[i]["y"_t]));
  }
  const Point& last = pts.back();
  PostMessageW(hwnd, WM_LBUTTONUP, 0, client_lparam(last["x"_t], last["y"_t]));
  return true;
}

WORD vk_from_name(const std::string& name) {
  static constexpr std::pair<std::string_view, WORD> kAlias[] = {
      {"ESCAPE", VK_ESCAPE}, {"Esc", VK_ESCAPE},   {"RETURN", VK_RETURN},
      {"ENTER", VK_RETURN},  {"TAB", VK_TAB},      {"SPACE", VK_SPACE},
      {"CTRL", VK_CONTROL},  {"CONTROL", VK_CONTROL},
      {"SHIFT", VK_SHIFT},   {"ALT", VK_MENU},     {"MENU", VK_MENU},
  };
  for (const auto& [alias, vk] : kAlias) {
    if (name == alias) {
      return vk;
    }
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

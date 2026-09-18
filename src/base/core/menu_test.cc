// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/core/api.h"
#include "base/core/listener.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

class TestListener : public base::SmtListener {
 public:
  TestListener() {
    set_name("view");
    append_func_items("Zoom In", 1001, base::FIM_2DVIEW | base::FIM_2DMFMENU);
    append_func_items("放大", 1002, base::FIM_2DVIEW | base::FIM_2DMFMENU);
  }
  int notify(long, base::SmtListenerMsg&) override { return 0; }
};

void test_popup_handle_not_truncated() {
  HMENU popup = ::CreatePopupMenu();
  expect(popup != nullptr, "CreatePopupMenu popup");
  expect(::AppendMenuA(popup, MF_STRING, 1, "item") != FALSE, "item");

  HMENU parent = ::CreatePopupMenu();
  expect(parent != nullptr, "CreatePopupMenu parent");
  expect(append_popup_menu(parent, popup, "view"), "append_popup_menu");

  MENUITEMINFOA info = {};
  info.cbSize = sizeof(info);
  info.fMask = MIIM_SUBMENU | MIIM_STRING;
  char caption[32] = {};
  info.dwTypeData = caption;
  info.cch = static_cast<UINT>(sizeof(caption) - 1);
  expect(::GetMenuItemInfoA(parent, 0, TRUE, &info) != FALSE, "GetMenuItemInfoA");
  expect(info.hSubMenu == popup, "submenu HMENU must be pointer-sized, not UINT");
  expect(std::strcmp(caption, "view") == 0, "popup caption");

  const UINT_PTR truncated = static_cast<UINT>(reinterpret_cast<UINT_PTR>(popup));
  const UINT_PTR stored = reinterpret_cast<UINT_PTR>(info.hSubMenu);
  if (truncated != reinterpret_cast<UINT_PTR>(popup)) {
    expect(stored != truncated, "stored handle must keep high bits");
  }

  ::DestroyMenu(parent);
}

void test_listener_menu_captions() {
  TestListener listener;
  HMENU menu = create_listener_menu(&listener, base::FIM_2DVIEW);
  expect(menu != nullptr, "create_listener_menu");
  expect(::GetMenuItemCount(menu) == 2, "two items");

  char zoom[64] = {};
  expect(::GetMenuStringA(menu, 1001, zoom, 64, MF_BYCOMMAND) > 0, "Zoom In get");
  expect(std::strcmp(zoom, "Zoom In") == 0, "Zoom In caption");

  char zh[64] = {};
  expect(::GetMenuStringA(menu, 1002, zh, 64, MF_BYCOMMAND) > 0, "放大 get");
  expect(std::strcmp(zh, "放大") == 0, "放大 caption (MBCS, not UTF-16)");

  HMENU owner = ::CreatePopupMenu();
  expect(attach_listener_popup(owner, &listener, base::FIM_2DMFMENU, listener.get_name()),
         "attach_listener_popup");
  MENUITEMINFOA info = {};
  info.cbSize = sizeof(info);
  info.fMask = MIIM_SUBMENU;
  expect(::GetMenuItemInfoA(owner, 0, TRUE, &info) != FALSE, "attached submenu");
  expect(info.hSubMenu != nullptr, "attached submenu handle");
  expect(::GetMenuItemCount(info.hSubMenu) == 2, "attached item count");

  ::DestroyMenu(owner);
  ::DestroyMenu(menu);
}

void test_null_listener_safe() {
  expect(create_listener_menu(nullptr, base::FIM_2DVIEW) == nullptr, "null listener");
  expect(!attach_listener_popup(nullptr, nullptr, base::FIM_2DVIEW, "x"),
         "null attach");
}

}  // namespace

int main() {
  test_popup_handle_not_truncated();
  test_listener_menu_captions();
  test_null_listener_safe();
  if (g_fails != 0) {
    std::fprintf(stderr, "menu_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("menu_test: ok\n");
  return 0;
}

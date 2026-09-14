// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cs/native/sg_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
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

}  // namespace

int main() {
  SgHost* first = sg_host_create();
  expect(first != nullptr, "create");
  SgHost* a = sg_host_create();
  expect(a != nullptr, "second create");
  sg_host_destroy(first);
  sg_host_destroy(a);
  sg_host_destroy(nullptr);

  SgHost* host = sg_host_create();
  expect(host != nullptr, "host");
  expect(sg_host_view_id(host) == 0, "no view yet");
  expect(sg_host_view_kind(host) == 0, "default kind map-edit");
  expect(!sg_host_is_oop(host), "oop before start");
  sg_host_catalog_call(host, "{\"op\":\"refresh\"}");
  sg_host_catalog_call(host, nullptr);
  sg_host_activate_tool(host, "view.pan");
  sg_host_activate_tool(host, nullptr);

  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"SgHostTestParent";
  RegisterClassExW(&wc);
  HWND parent = CreateWindowExW(0, L"SgHostTestParent", L"sg_host_test",
                                WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
  expect(parent != nullptr, "parent hwnd");
  ShowWindow(parent, SW_SHOW);
  sg_host_attach_parent(host, parent);
  sg_host_sync_layout(host, 8, 8, 320, 240, 96.f);
  HWND child = static_cast<HWND>(sg_host_map_child_hwnd(host));
  expect(child != nullptr && IsWindow(child), "child hwnd");
  expect(sg_host_has_synced_layout(host) == 1, "synced layout");

  const uint32_t map_id = sg_host_open_view(host, 0);
  expect(map_id != 0, "open map");
  expect(sg_host_view_id(host) == map_id, "view id after open");
  sg_host_show_kind(host, 1);
  expect(sg_host_view_kind(host) == 1, "show data");
  sg_host_show_kind(host, 2);
  expect(sg_host_view_kind(host) == 2, "show 3d");
  sg_host_show_kind(host, 0);
  expect(sg_host_view_kind(host) == 0, "show map");
  sg_host_set_visible(host, 0);
  sg_host_set_visible(host, 1);

  sg_host_destroy(host);
  DestroyWindow(parent);

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("sg_host_test ok\n");
  return 0;
}

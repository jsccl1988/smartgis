// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cs/native/sg_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
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
  // Headless HWND fuzz repeatedly attaches/releases FlyCube DX12; that path
  // has known heap/stack corruption on teardown. Force ContentMapView + GDI
  // for this smoke (product default remains FlyCube / present_gpu).
  _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_3D", "1");

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

  // WinUI names its island DesktopChildSiteBridge. Parenting the map HWND
  // into that island is what WER 0x80070578 (ERROR_INVALID_WINDOW_HANDLE)
  // hits when the user clicks chrome and WinUI recreates the bridge.
  WNDCLASSEXW island_wc;
  ZeroMemory(&island_wc, sizeof(island_wc));
  island_wc.cbSize = sizeof(island_wc);
  island_wc.lpfnWndProc = DefWindowProcW;
  island_wc.hInstance = GetModuleHandleW(nullptr);
  island_wc.lpszClassName = L"DesktopChildSiteBridge.Fake";
  RegisterClassExW(&island_wc);
  HWND island = CreateWindowExW(0, L"DesktopChildSiteBridge.Fake", L"",
                                WS_CHILD | WS_VISIBLE, 0, 0, 640, 480, parent,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
  expect(island != nullptr, "fake xaml island");

  sg_host_attach_parent(host, parent);
  sg_host_sync_layout(host, 8, 8, 320, 240, 96.f);
  HWND child = static_cast<HWND>(sg_host_map_child_hwnd(host));
  expect(child != nullptr && IsWindow(child), "child hwnd");
  expect(GetParent(child) == parent, "map hwnd parented to top-level, not island");
  expect(sg_host_has_synced_layout(host) == 1, "synced layout");

  const uint32_t map_id = sg_host_open_view(host, 0);
  expect(map_id != 0, "open map");
  expect(sg_host_view_id(host) == map_id, "view id after open");
  sg_host_show_kind(host, 1);
  expect(sg_host_view_kind(host) == 1, "show data");
  sg_host_show_kind(host, 2);
  expect(sg_host_view_kind(host) == 2, "show 3d");
  // Scene3dController WinUI parity: trackball + wheel must not kill the HWND.
  sg_host_activate_tool(host, "view3d.trackball");
  sg_host_dispatch_pointer(host, 2, 40, 48, 0);   // LDown
  sg_host_dispatch_pointer(host, 0, 64, 72, 0);   // Move
  sg_host_dispatch_pointer(host, 3, 64, 72, 0);   // LUp
  sg_host_dispatch_pointer(host, 1, 40, 48, 120); // Wheel
  RedrawWindow(child, nullptr, nullptr,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
  expect(IsWindow(child), "3d child live after scene3d gestures");
  expect(sg_host_view_kind(host) == 2, "still 3d after gestures");
  sg_host_show_kind(host, 0);
  expect(sg_host_view_kind(host) == 0, "show map");
  expect(static_cast<HWND>(sg_host_map_child_hwnd(host)) == child,
         "hwnd stable across show_kind");
  expect(IsWindow(child), "child still live after show_kind");

  DestroyWindow(island);
  island = CreateWindowExW(0, L"DesktopChildSiteBridge.Fake", L"",
                           WS_CHILD | WS_VISIBLE, 0, 0, 640, 480, parent,
                           nullptr, GetModuleHandleW(nullptr), nullptr);
  expect(island != nullptr, "recreated fake island");
  expect(IsWindow(child), "map hwnd survives island recreate");
  expect(GetParent(child) == parent, "still top-level after island recreate");

  for (int i = 0; i < 40; ++i) {
    sg_host_show_kind(host, i % 3);
    sg_host_sync_layout(host, 8, 8, 300 + (i % 7), 220 + (i % 5), 96.f);
    SendMessageW(child, WM_LBUTTONDOWN, 0, MAKELPARAM(12, 16));
    SendMessageW(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(18, 22));
    SendMessageW(child, WM_LBUTTONUP, 0, MAKELPARAM(18, 22));
    SendMessageW(child, WM_RBUTTONDOWN, 0, MAKELPARAM(24, 20));
    SendMessageW(child, WM_RBUTTONUP, 0, MAKELPARAM(24, 20));
    sg_host_activate_tool(host, (i % 2) ? "selection.point" : "view.pan");
    sg_host_dispatch_pointer(host, 2, 20, 24, 0);
    sg_host_dispatch_pointer(host, 0, 28, 30, 0);
    sg_host_dispatch_pointer(host, 3, 28, 30, 0);
    sg_host_dispatch_pointer(host, 1, 20, 24, 120);
    sg_host_catalog_call(host, "{\"op\":\"refresh\"}");
  }
  expect(IsWindow(child), "child live after click fuzz");
  expect(static_cast<HWND>(sg_host_map_child_hwnd(host)) == child,
         "hwnd identity stable after click fuzz");
  expect(GetParent(child) == parent, "parent still top-level after click fuzz");

  sg_host_set_visible(host, 0);
  sg_host_set_visible(host, 1);

  sg_host_destroy(host);
  DestroyWindow(island);
  DestroyWindow(parent);

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("sg_host_test ok\n");
  return 0;
}

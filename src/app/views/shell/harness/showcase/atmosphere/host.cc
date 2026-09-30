// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/atmosphere/host.h"

namespace app {
namespace detail {
namespace {

constexpr wchar_t kAtmosphereShowcaseClass[] = L"SmartGisAtmosphereShowcase";

LRESULT CALLBACK atmosphere_showcase_wnd_proc(HWND hwnd, UINT msg, WPARAM wp,
                                              LPARAM lp) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
      return 0;
    }
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

}  // namespace

HWND create_atmosphere_showcase_hwnd(uint32_t width_px, uint32_t height_px) {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = atmosphere_showcase_wnd_proc;
  wc.hInstance = inst;
  wc.lpszClassName = kAtmosphereShowcaseClass;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassExW(&wc);

  RECT wr = {0, 0, static_cast<LONG>(width_px), static_cast<LONG>(height_px)};
  AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
  HWND hwnd = CreateWindowExW(
      WS_EX_APPWINDOW, kAtmosphereShowcaseClass,
      L"SmartGIS Atmosphere Showcase",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT,
      CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr,
      inst, nullptr);
  if (!hwnd) {
    return nullptr;
  }
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  SetForegroundWindow(hwnd);
  return hwnd;
}

}  // namespace detail
}  // namespace app

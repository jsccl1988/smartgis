// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/window.h"

#include <imm.h>

#pragma comment(lib, "imm32.lib")

namespace app {
namespace detail {
namespace {

LRESULT CALLBACK showcase_present_wnd_proc(HWND hwnd, UINT msg, WPARAM wp,
                                           LPARAM lp) {
  // Swallow C++ EH from third-party window hooks (TSF) so CreateWindow /
  // DispatchMessage cannot escalate to STATUS_FATAL_USER_CALLBACK_EXCEPTION.
  try {
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
  } catch (...) {
    return 0;
  }
}

}  // namespace

HWND create_showcase_present_hwnd(const ShowcasePresentHwndOpts& opts) {
  if (!opts.class_name || !opts.window_title || opts.width_px < 8 ||
      opts.height_px < 8) {
    return nullptr;
  }
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = showcase_present_wnd_proc;
  wc.hInstance = inst;
  wc.lpszClassName = opts.class_name;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassExW(&wc);

  RECT wr = {0, 0, static_cast<LONG>(opts.width_px),
             static_cast<LONG>(opts.height_px)};
  AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
  // WS_EX_NOACTIVATE + SHOWNOACTIVATE: avoid foreground focus that re-arms
  // TSF/IME callbacks during harness CaptureWindow creation.
  HWND hwnd = CreateWindowExW(
      WS_EX_NOACTIVATE, opts.class_name, opts.window_title,
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, inst, nullptr);
  if (!hwnd) {
    return nullptr;
  }
  ImmAssociateContext(hwnd, nullptr);
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd);
  return hwnd;
}

void destroy_showcase_present_hwnd(HWND* hwnd) {
  if (!hwnd || !*hwnd) {
    return;
  }
  DestroyWindow(*hwnd);
  *hwnd = nullptr;
}

}  // namespace detail
}  // namespace app

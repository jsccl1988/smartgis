// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/chrome/identity_hud.h"

namespace ui {
namespace views {
namespace detail {

const wchar_t kIdentityHudClass[] = L"SmartGisMapIdentityHud";

namespace {

LRESULT CALLBACK identity_hud_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
    const wchar_t* text =
        reinterpret_cast<const wchar_t*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (text && text[0]) {
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(255, 255, 0));
      TextOutW(hdc, 8, 6, text, lstrlenW(text));
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_NCHITTEST) {
    // Map / orbit input hits the parent under the HUD.
    return HTTRANSPARENT;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

void register_identity_hud_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = identity_hud_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kIdentityHudClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace detail
}  // namespace views
}  // namespace ui

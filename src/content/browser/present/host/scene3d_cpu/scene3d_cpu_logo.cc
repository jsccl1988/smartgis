// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/hdc/scene3d_hdc_logo.h"

#include "content/browser/present/host/gdi/gdi_primitives.h"

#include <cstdio>
#include <cwchar>

namespace content {
namespace detail {
namespace {

constexpr wchar_t kEngineLogoClass[] = L"SmartGisEngineLogoBadge";
constexpr int kEngineLogoPadX = 10;
constexpr int kEngineLogoPadY = 6;
constexpr int kEngineLogoMargin = 14;

LRESULT CALLBACK engine_logo_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                      LPARAM lparam) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    const char* name =
        reinterpret_cast<const char*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    paint_engine_logo_at(hdc, 0, 0, name);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_NCHITTEST) {
    // Let map / orbit gestures hit the parent under the badge.
    return HTTRANSPARENT;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void register_engine_logo_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = engine_logo_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kEngineLogoClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace

bool measure_engine_logo(HDC hdc, const char* engine_name, SIZE* out_box) {
  if (!hdc || !out_box) {
    return false;
  }
  const char* name =
      (engine_name && engine_name[0]) ? engine_name : "unknown";
  wchar_t wname[96];
  swprintf_s(wname, L"%hs", name);
  SIZE sz = {};
  if (!GetTextExtentPoint32W(hdc, wname, lstrlenW(wname), &sz)) {
    return false;
  }
  out_box->cx = sz.cx + kEngineLogoPadX * 2;
  out_box->cy = sz.cy + kEngineLogoPadY * 2;
  return out_box->cx > 0 && out_box->cy > 0;
}

void paint_engine_logo_at(HDC hdc, int x, int y, const char* engine_name) {
  if (!hdc) {
    return;
  }
  const char* name =
      (engine_name && engine_name[0]) ? engine_name : "unknown";
  wchar_t wname[96];
  swprintf_s(wname, L"%hs", name);
  SIZE box = {};
  if (!measure_engine_logo(hdc, name, &box)) {
    return;
  }
  gdi_fill_rect(hdc, x, y, box.cx, box.cy, RGB(18, 26, 34));
  {
    ScopedGdiPen pen(hdc, RGB(90, 110, 130), 1);
    ScopedGdiSelect null_brush(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + box.cx, y + box.cy);
  }
  gdi_draw_text_w(hdc, x + kEngineLogoPadX, y + kEngineLogoPadY, wname,
                  lstrlenW(wname), RGB(245, 248, 252));
}

void paint_engine_logo_corner(HDC hdc, int width_px, int height_px,
                              const char* engine_name) {
  if (!hdc || width_px < 48 || height_px < 28) {
    return;
  }
  SIZE box = {};
  if (!measure_engine_logo(hdc, engine_name, &box)) {
    return;
  }
  const int x = width_px - kEngineLogoMargin - box.cx;
  const int y = height_px - kEngineLogoMargin - box.cy;
  if (x < 4 || y < 4) {
    return;
  }
  paint_engine_logo_at(hdc, x, y, engine_name);
}

void hide_engine_logo_overlay(HWND* logo_hwnd) {
  if (!logo_hwnd) {
    return;
  }
  if (*logo_hwnd && IsWindow(*logo_hwnd)) {
    ShowWindow(*logo_hwnd, SW_HIDE);
  }
}

void release_engine_logo_overlay(HWND* logo_hwnd) {
  if (!logo_hwnd) {
    return;
  }
  HWND hwnd = *logo_hwnd;
  *logo_hwnd = nullptr;
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
  // Parent teardown already destroys children; avoid DestroyWindow on a
  // half-torn-down hierarchy (exit AV on Views/WinUI smoke).
  HWND parent = GetParent(hwnd);
  if (parent && IsWindow(parent)) {
    DestroyWindow(hwnd);
  }
}

void sync_engine_logo_overlay(HWND* logo_hwnd, HWND parent, int width_px,
                              int height_px, const char* label) {
  if (!logo_hwnd) {
    return;
  }
  if (!parent || !IsWindow(parent) || width_px < 48 || height_px < 28) {
    hide_engine_logo_overlay(logo_hwnd);
    return;
  }
  register_engine_logo_class();
  HDC probe = GetDC(parent);
  if (!probe) {
    return;
  }
  SIZE box = {};
  const bool measured = measure_engine_logo(probe, label, &box);
  ReleaseDC(parent, probe);
  if (!measured) {
    return;
  }
  const int x = width_px - kEngineLogoMargin - box.cx;
  const int y = height_px - kEngineLogoMargin - box.cy;
  if (x < 4 || y < 4) {
    hide_engine_logo_overlay(logo_hwnd);
    return;
  }
  if (!*logo_hwnd || !IsWindow(*logo_hwnd) || GetParent(*logo_hwnd) != parent) {
    release_engine_logo_overlay(logo_hwnd);
    *logo_hwnd = CreateWindowExW(
        WS_EX_NOACTIVATE, kEngineLogoClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, x, y, box.cx, box.cy, parent,
        nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!*logo_hwnd) {
      return;
    }
  } else {
    SetWindowPos(*logo_hwnd, HWND_TOP, x, y, box.cx, box.cy,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
  SetWindowLongPtrW(*logo_hwnd, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(label));
  // Invalidate only — UpdateWindow here re-enters while the parent is still
  // inside BeginPaint/EndPaint (FlyCube / stereo present paths).
  InvalidateRect(*logo_hwnd, nullptr, FALSE);
}

}  // namespace detail
}  // namespace content

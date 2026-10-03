// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/chrome/identity_hud.h"

#include <cstring>

namespace ui {
namespace detail {

void paint_identity_hud(HWND hwnd, HDC hdc, const char* label) {
  if (!hwnd || !label) {
    return;
  }
  HDC paint = hdc;
  const bool release = !paint;
  if (!paint) {
    paint = ::GetDC(hwnd);
  }
  if (!paint) {
    return;
  }
  RECT bar = {0, 0, 0, kIdentityHudHeight};
  ::GetClientRect(hwnd, &bar);
  bar.bottom = kIdentityHudHeight;
  HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
  ::FillRect(paint, &bar, brush);
  ::DeleteObject(brush);
  ::SetBkMode(paint, TRANSPARENT);
  ::SetTextColor(paint, RGB(255, 255, 0));
  ::TextOutA(paint, 8, 6, label, static_cast<int>(std::strlen(label)));
  if (release) {
    ::ReleaseDC(hwnd, paint);
  }
}

}  // namespace detail
}  // namespace ui

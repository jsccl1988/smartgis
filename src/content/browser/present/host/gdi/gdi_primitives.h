// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_HOST_GDI_GDI_PRIMITIVES_H_
#define CONTENT_BROWSER_PRESENT_HOST_GDI_GDI_PRIMITIVES_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <windows.h>

namespace content {
namespace detail {

// Thin Win32 helpers for blit preview / ocean fill. Not a present SoT.

// 0xAARRGGBB -> COLORREF.
inline COLORREF rgba_to_colorref(uint32_t rgba) {
  return RGB(static_cast<uint8_t>((rgba >> 16) & 0xff),
             static_cast<uint8_t>((rgba >> 8) & 0xff),
             static_cast<uint8_t>(rgba & 0xff));
}

inline void gdi_fill_rect(HDC hdc, const RECT& rc, COLORREF color) {
  if (!hdc) {
    return;
  }
  HBRUSH brush = ::CreateSolidBrush(color);
  if (!brush) {
    return;
  }
  ::FillRect(hdc, &rc, brush);
  ::DeleteObject(brush);
}

inline void gdi_fill_rect(HDC hdc, int x, int y, int w, int h, COLORREF color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  RECT rc{x, y, x + w, y + h};
  gdi_fill_rect(hdc, rc, color);
}

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_HOST_GDI_GDI_PRIMITIVES_H_

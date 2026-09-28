// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/font/font.h"

namespace ui {
namespace gfx {

Size measure_text_with_font(const Font& font, const wchar_t* text) {
  if (!text || !text[0]) {
    return Size{};
  }

  HDC screen = GetDC(nullptr);
  if (!screen) {
    const int chars = lstrlenW(text);
    return Size{chars * (font.size_px / 2 + 1), font.size_px + 4};
  }

  const int weight = font.bold ? FW_BOLD : FW_NORMAL;
  HFONT hf = CreateFontW(-font.size_px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                         font.family.c_str());
  HGDIOBJ old = nullptr;
  if (hf) {
    old = SelectObject(screen, hf);
  }

  SIZE sz = {};
  GetTextExtentPoint32W(screen, text, lstrlenW(text), &sz);

  if (hf) {
    SelectObject(screen, old);
    DeleteObject(hf);
  }
  ReleaseDC(nullptr, screen);
  return Size{sz.cx, sz.cy};
}

}  // namespace gfx
}  // namespace ui

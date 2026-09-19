// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/theme.h"

#include <windows.h>

#include "render/skia/canvas.h"

namespace ui {
namespace views {

const Theme& Theme::current() {
  static const Theme kDark;
  return kDark;
}

std::wstring utf8_to_wide(const std::string& u8) {
  if (u8.empty()) {
    return L"";
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
  std::wstring w(n > 0 ? static_cast<size_t>(n - 1) : 0, L'\0');
  if (n > 1) {
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), n);
  }
  return w;
}

std::string wide_to_utf8(const wchar_t* w) {
  if (!w || !w[0]) {
    return {};
  }
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  std::string s(n > 0 ? static_cast<size_t>(n - 1) : 0, '\0');
  if (n > 1) {
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  }
  return s;
}

Size measure_text_utf8(const std::string& text) {
  Size out;
  if (text.empty()) {
    return out;
  }
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return out;
  }
  // Fixed 12px Segoe UI so ink is DIP-stable; callers scale via dip_to_px /
  // device_scale_factor rather than inheriting process DPI quirks on the DC.
  HFONT font =
      CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HGDIOBJ old = font ? SelectObject(screen, font) : nullptr;
  render::skia::Canvas canvas(screen, 1, 1);
  const render::skia::Size ink =
      canvas.measure_text(utf8_to_wide(text).c_str());
  if (font) {
    SelectObject(screen, old);
    DeleteObject(font);
  }
  ReleaseDC(nullptr, screen);
  out.width = ink.width;
  out.height = ink.height;
  return out;
}

void draw_focus_ring(render::skia::Canvas* canvas, const Rect& bounds) {
  if (!canvas) {
    return;
  }
  canvas->stroke_rect(bounds.x, bounds.y, bounds.width, bounds.height,
                      Theme::current().accent, 1);
}

}  // namespace views
}  // namespace ui

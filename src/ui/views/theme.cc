// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/theme.h"

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

void draw_focus_ring(render::skia::Canvas* canvas, const Rect& bounds) {
  if (!canvas) {
    return;
  }
  const render::skia::Color c = Theme::current().accent;
  canvas->fill_rect(bounds.x, bounds.y, bounds.width, 1, c);
  canvas->fill_rect(bounds.x, bounds.bottom() - 1, bounds.width, 1, c);
  canvas->fill_rect(bounds.x, bounds.y, 1, bounds.height, c);
  canvas->fill_rect(bounds.right() - 1, bounds.y, 1, bounds.height, c);
}

}  // namespace views
}  // namespace ui

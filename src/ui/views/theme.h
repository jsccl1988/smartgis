// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_THEME_H_
#define UI_VIEWS_THEME_H_

#include <string>

#include "render/skia/color.h"
#include "ui/views/view.h"

namespace render {
namespace skia {
class Canvas;
}
}  // namespace render

namespace ui {
namespace views {

// Dark chrome colors matching the Views catalog blues/grays.
struct Theme {
  render::skia::Color chrome_bg = render::skia::color_rgb(30, 30, 30);
  render::skia::Color panel_bg = render::skia::color_rgb(37, 37, 38);
  render::skia::Color panel_header = render::skia::color_rgb(45, 45, 48);
  render::skia::Color accent = render::skia::color_rgb(0, 122, 204);
  render::skia::Color text = render::skia::color_rgb(200, 200, 200);
  render::skia::Color text_bright = render::skia::color_rgb(255, 255, 255);
  render::skia::Color text_muted = render::skia::color_rgb(140, 140, 140);
  render::skia::Color control_bg = render::skia::color_rgb(30, 30, 30);
  render::skia::Color control_fill = render::skia::color_rgb(60, 60, 60);
  render::skia::Color control_hover = render::skia::color_rgb(80, 80, 80);
  render::skia::Color control_press = render::skia::color_rgb(50, 50, 50);
  render::skia::Color control_disabled = render::skia::color_rgb(45, 45, 45);
  render::skia::Color control_unchecked = render::skia::color_rgb(50, 50, 50);
  render::skia::Color map_placeholder = render::skia::color_rgb(27, 58, 75);

  static const Theme& current();
};

std::wstring utf8_to_wide(const std::string& u8);
std::string wide_to_utf8(const wchar_t* w);

void draw_focus_ring(render::skia::Canvas* canvas, const Rect& bounds);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_THEME_H_

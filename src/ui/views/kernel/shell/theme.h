// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_SHELL_THEME_H_
#define UI_VIEWS_KERNEL_SHELL_THEME_H_

#include "ui/ui_views_export.h"
#include <string>

#include "ui/gfx/color/color.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Dark shell colors matching the Views catalog blues/grays.
struct Theme {
  ui::gfx::Color shell_bg = ui::gfx::color_rgb(30, 30, 30);
  ui::gfx::Color panel_bg = ui::gfx::color_rgb(37, 37, 38);
  ui::gfx::Color panel_header = ui::gfx::color_rgb(45, 45, 48);
  ui::gfx::Color accent = ui::gfx::color_rgb(0, 122, 204);
  ui::gfx::Color text = ui::gfx::color_rgb(200, 200, 200);
  ui::gfx::Color text_bright = ui::gfx::color_rgb(255, 255, 255);
  ui::gfx::Color text_muted = ui::gfx::color_rgb(140, 140, 140);
  ui::gfx::Color control_bg = ui::gfx::color_rgb(30, 30, 30);
  ui::gfx::Color control_fill = ui::gfx::color_rgb(60, 60, 60);
  ui::gfx::Color control_hover = ui::gfx::color_rgb(80, 80, 80);
  ui::gfx::Color control_press = ui::gfx::color_rgb(50, 50, 50);
  ui::gfx::Color control_disabled = ui::gfx::color_rgb(45, 45, 45);
  ui::gfx::Color control_unchecked = ui::gfx::color_rgb(50, 50, 50);
  ui::gfx::Color map_placeholder = ui::gfx::color_rgb(27, 58, 75);

  static UI_VIEWS_EXPORT const Theme& current();
};

UI_VIEWS_EXPORT std::wstring utf8_to_wide(const std::string& u8);
UI_VIEWS_EXPORT std::string wide_to_utf8(const wchar_t* w);

// Ink size at |device_scale| (1 = 96 DPI). The returned pixels are already
// device pixels for a 12 DIP Segoe UI face — do not multiply by scale again.
UI_VIEWS_EXPORT Size measure_text_utf8(const std::string& text);
UI_VIEWS_EXPORT Size measure_text_utf8(const std::string& text, float device_scale);

UI_VIEWS_EXPORT void draw_focus_ring(ui::gfx::Canvas* canvas, const Rect& bounds);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_SHELL_THEME_H_

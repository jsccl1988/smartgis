// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_SHELL_THEME_H_
#define UI_VIEWS_KERNEL_SHELL_THEME_H_

#include "ui/ui_export.h"
#include <string>

#include "ui/gfx/color/color.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Shell color snapshot for the active ThemeService pack.
struct Theme {
  ui::gfx::Color shell_bg = ui::gfx::color_rgb(30, 30, 30);
  ui::gfx::Color panel_bg = ui::gfx::color_rgb(37, 37, 38);
  ui::gfx::Color panel_header = ui::gfx::color_rgb(45, 45, 48);
  ui::gfx::Color accent = ui::gfx::color_rgb(0, 122, 204);
  ui::gfx::Color text = ui::gfx::color_rgb(235, 235, 235);
  ui::gfx::Color text_bright = ui::gfx::color_rgb(255, 255, 255);
  ui::gfx::Color text_muted = ui::gfx::color_rgb(175, 175, 175);
  ui::gfx::Color control_bg = ui::gfx::color_rgb(30, 30, 30);
  ui::gfx::Color control_fill = ui::gfx::color_rgb(55, 55, 58);
  ui::gfx::Color control_hover = ui::gfx::color_rgb(78, 78, 82);
  ui::gfx::Color control_press = ui::gfx::color_rgb(48, 48, 52);
  ui::gfx::Color control_disabled = ui::gfx::color_rgb(45, 45, 45);
  ui::gfx::Color control_unchecked = ui::gfx::color_rgb(50, 50, 50);
  ui::gfx::Color map_placeholder = ui::gfx::color_rgb(27, 58, 75);
  ui::gfx::Color caption_bg = ui::gfx::color_rgb(37, 37, 38);
  ui::gfx::Color caption_button_hover = ui::gfx::color_rgb(60, 60, 60);
  ui::gfx::Color caption_close_hover = ui::gfx::color_rgb(196, 43, 28);

  static UI_EXPORT const Theme& current();
};

UI_EXPORT std::wstring utf8_to_wide(const std::string& u8);
UI_EXPORT std::string wide_to_utf8(const wchar_t* w);

// Shell chrome body face (DIP). Commit + measure + GDI raster share this.
// 20 DIP keeps catalog/menu glyphs readable on 200–250% hosts once row
// heights also scale (fixed-px rows used to clip a larger face).
inline constexpr int kShellBodyFontDip = 20;

// Ink size at |device_scale| (1 = 96 DPI). The returned pixels are already
// device pixels for kShellBodyFontDip Segoe UI — do not multiply by scale again.
UI_EXPORT Size measure_text_utf8(const std::string& text);
UI_EXPORT Size measure_text_utf8(const std::string& text, float device_scale);

// Physical pixel height of the shell body face at |device_scale|.
inline int shell_body_font_px(float device_scale) {
  return dip_to_px(kShellBodyFontDip, device_scale);
}

UI_EXPORT void draw_focus_ring(ui::gfx::Canvas* canvas, const Rect& bounds);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_SHELL_THEME_H_

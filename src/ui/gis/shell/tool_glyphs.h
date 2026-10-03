// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_SHELL_TOOL_GLYPHS_H_
#define UI_GIS_SHELL_TOOL_GLYPHS_H_

#include <algorithm>
#include <string_view>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/kernel/shell/theme.h"

namespace ui {
namespace views {
namespace detail {

inline bool glyph_id_starts(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

inline bool glyph_id_has(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

// Vector tool glyphs for Ambox chips (line geometry only — no image assets).
// Sized to |box|; stroke scales with box so high-DPI stays crisp.
inline void paint_tool_glyph(ui::gfx::Canvas* canvas,
                             const Rect& box,
                             std::string_view command_id,
                             ui::gfx::Color ink,
                             bool muted) {
  if (!canvas || box.width <= 0 || box.height <= 0) {
    return;
  }
  const ui::gfx::Color c = muted ? Theme::current().text_muted : ink;
  const int stroke = std::max(1, box.width / 8);
  const int inset = std::max(1, box.width / 5);
  const int cx = box.x + box.width / 2;
  const int cy = box.y + box.height / 2;
  const int half = std::max(2, box.width / 2 - inset);

  if (command_id == "select" || glyph_id_starts(command_id, "selection.")) {
    const int tip_x = box.x + inset;
    const int tip_y = box.y + inset;
    const int base_x = box.x + box.width - inset;
    const int base_y = box.y + box.height - inset;
    canvas->draw_line(tip_x, tip_y, base_x, base_y, c, stroke);
    canvas->draw_line(tip_x, tip_y, tip_x + half, tip_y, c, stroke);
    canvas->draw_line(tip_x, tip_y, tip_x, tip_y + half, c, stroke);
    return;
  }
  if (command_id == "identify") {
    canvas->stroke_rect(box.x + inset, box.y + inset, box.width - 2 * inset,
                        box.height - 2 * inset, c, stroke);
    canvas->fill_rect(cx - stroke, box.y + inset + stroke, stroke * 2,
                      stroke * 2, c);
    canvas->draw_line(cx, cy - stroke, cx, box.y + box.height - inset - stroke,
                      c, stroke);
    return;
  }
  if (command_id == "edit.append.point" ||
      (glyph_id_starts(command_id, "edit.") && glyph_id_has(command_id, "point"))) {
    const int r = std::max(2, half / 2);
    canvas->fill_rect(cx - r, cy - r, r * 2, r * 2, c);
    return;
  }
  if (command_id == "edit.append.linestring" ||
      (glyph_id_starts(command_id, "edit.") &&
       (glyph_id_has(command_id, "linestring") ||
        glyph_id_has(command_id, "line")))) {
    canvas->draw_line(box.x + inset, box.y + box.height - inset, cx,
                      box.y + inset, c, stroke);
    canvas->draw_line(cx, box.y + inset, box.x + box.width - inset,
                      box.y + box.height - inset, c, stroke);
    return;
  }
  if (command_id == "edit.append.polygon" ||
      glyph_id_has(command_id, "polygon")) {
    const int top = box.y + inset;
    const int bot = box.y + box.height - inset;
    const int left = box.x + inset;
    const int right = box.x + box.width - inset;
    canvas->draw_line(cx, top, right, bot, c, stroke);
    canvas->draw_line(right, bot, left, bot, c, stroke);
    canvas->draw_line(left, bot, cx, top, c, stroke);
    return;
  }
  if (command_id == "edit.undo" || glyph_id_has(command_id, "undo")) {
    canvas->draw_line(box.x + inset, cy, box.x + box.width - inset, cy, c,
                      stroke);
    canvas->draw_line(box.x + inset, cy, box.x + inset + half / 2, cy - half / 2,
                      c, stroke);
    canvas->draw_line(box.x + inset, cy, box.x + inset + half / 2, cy + half / 2,
                      c, stroke);
    return;
  }
  if (command_id == "edit.cancel" || glyph_id_has(command_id, "cancel")) {
    canvas->draw_line(box.x + inset, box.y + inset, box.x + box.width - inset,
                      box.y + box.height - inset, c, stroke);
    canvas->draw_line(box.x + box.width - inset, box.y + inset, box.x + inset,
                      box.y + box.height - inset, c, stroke);
    return;
  }
  if (glyph_id_starts(command_id, "measure") ||
      glyph_id_has(command_id, "measure")) {
    canvas->draw_line(box.x + inset, cy, box.x + box.width - inset, cy, c,
                      stroke);
    for (int i = 0; i < 3; ++i) {
      const int tx = box.x + inset + (i + 1) * (box.width - 2 * inset) / 4;
      canvas->draw_line(tx, cy - half / 2, tx, cy + half / 2, c, stroke);
    }
    return;
  }
  canvas->stroke_rect(box.x + inset, box.y + inset, box.width - 2 * inset,
                      box.height - 2 * inset, c, stroke);
  canvas->draw_line(box.x + inset + 1, box.y + box.height - inset - 1,
                    box.x + box.width - inset - 1, box.y + inset + 1, c, stroke);
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_GIS_SHELL_TOOL_GLYPHS_H_

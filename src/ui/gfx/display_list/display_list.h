// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_DISPLAY_LIST_DISPLAY_LIST_H_
#define UI_GFX_DISPLAY_LIST_DISPLAY_LIST_H_

#include "ui/ui_export.h"
#include <cstdint>
#include <string>
#include <vector>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"

namespace ui {
namespace gfx {

class Canvas;
class DisplayList;

UI_EXPORT void display_list_begin(DisplayList* list);
UI_EXPORT void display_list_end();
UI_EXPORT DisplayList* display_list_recorder();
// A view records when its paint inputs change; a later dirty rect replays
// only commands that intersect that rect. This is not a second widget tree.
// The command list stays in the header so it is not a separate container type.
class UI_EXPORT DisplayList {
 public:
  void clear() {
    cmds_.clear();
    text_.clear();
  }
  bool empty() const { return cmds_.empty(); }

  // Copy-on-commit helper: appends |other| so later mutations of |other| do
  // not affect this list. Remaps text indices into this list's string table.
  void append_from(const DisplayList& other);

  // Deep copy for an immutable committed snapshot.
  DisplayList clone() const;

  void fill_rect(int x, int y, int w, int h, Color color) {
    if (w <= 0 || h <= 0) {
      return;
    }
    Cmd cmd;
    cmd.op = Op::kFill;
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
    cmd.color = color;
    cmds_.push_back(cmd);
  }

  void stroke_rect(int x, int y, int w, int h, Color color, int stroke_width) {
    if (w <= 0 || h <= 0) {
      return;
    }
    Cmd cmd;
    cmd.op = Op::kStroke;
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
    cmd.color = color;
    cmd.stroke = stroke_width < 1 ? 1 : stroke_width;
    cmds_.push_back(cmd);
  }

  void draw_line(int x0, int y0, int x1, int y1, Color color, int stroke_width) {
    Cmd cmd;
    cmd.op = Op::kLine;
    cmd.x = x0;
    cmd.y = y0;
    cmd.x1 = x1;
    cmd.y1 = y1;
    cmd.color = color;
    cmd.stroke = stroke_width < 1 ? 1 : stroke_width;
    cmds_.push_back(cmd);
  }

  void draw_text(int x, int y, const wchar_t* text, Color color) {
    if (!text || !text[0]) {
      return;
    }
    Cmd cmd;
    cmd.op = Op::kText;
    cmd.x = x;
    cmd.y = y;
    const int chars = lstrlenW(text);
    cmd.w = chars * 32 + 4;
    cmd.h = 48;
    cmd.color = color;
    cmd.text = static_cast<std::uint32_t>(text_.size());
    text_.emplace_back(text);
    cmds_.push_back(cmd);
  }

  void replay(Canvas* canvas) const {
    if (!canvas) {
      return;
    }
    DisplayList* saved = display_list_recorder();
    display_list_begin(nullptr);
    for (const Cmd& cmd : cmds_) {
      replay_cmd(canvas, cmd);
    }
    display_list_begin(saved);
  }

  void replay_clipped(Canvas* canvas, int l, int t, int r, int b) const {
    if (!canvas || r <= l || b <= t) {
      return;
    }
    DisplayList* saved = display_list_recorder();
    display_list_begin(nullptr);
    for (const Cmd& cmd : cmds_) {
      if (!hits(cmd, l, t, r, b)) {
        continue;
      }
      replay_cmd(canvas, cmd);
    }
    display_list_begin(saved);
  }

 private:
  enum class Op : std::uint8_t { kFill, kStroke, kLine, kText };

  struct Cmd {
    Op op = Op::kFill;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int x1 = 0;
    int y1 = 0;
    Color color = 0;
    int stroke = 1;
    std::uint32_t text = 0;
  };

  static int lesser(int a, int b) { return a < b ? a : b; }
  static int greater(int a, int b) { return a > b ? a : b; }

  bool hits(const Cmd& cmd, int l, int t, int r, int b) const {
    int x0 = cmd.x;
    int y0 = cmd.y;
    int x1 = cmd.x + cmd.w;
    int y1 = cmd.y + cmd.h;
    if (cmd.op == Op::kLine) {
      x0 = lesser(cmd.x, cmd.x1);
      y0 = lesser(cmd.y, cmd.y1);
      x1 = greater(cmd.x, cmd.x1) + cmd.stroke;
      y1 = greater(cmd.y, cmd.y1) + cmd.stroke;
    }
    return x0 < r && x1 > l && y0 < b && y1 > t;
  }

  void replay_cmd(Canvas* canvas, const Cmd& cmd) const {
    switch (cmd.op) {
      case Op::kFill:
        canvas->fill_rect(cmd.x, cmd.y, cmd.w, cmd.h, cmd.color);
        break;
      case Op::kStroke:
        canvas->stroke_rect(cmd.x, cmd.y, cmd.w, cmd.h, cmd.color, cmd.stroke);
        break;
      case Op::kLine:
        canvas->draw_line(cmd.x, cmd.y, cmd.x1, cmd.y1, cmd.color, cmd.stroke);
        break;
      case Op::kText:
        if (cmd.text < text_.size()) {
          canvas->draw_text(cmd.x, cmd.y, text_[cmd.text].c_str(), cmd.color);
        }
        break;
    }
  }

  std::vector<Cmd> cmds_;
  std::vector<std::wstring> text_;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_DISPLAY_LIST_DISPLAY_LIST_H_


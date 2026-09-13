// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/label.h"

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

Label::Label(std::string text) : text_(std::move(text)) {
  set_preferred_size({160, 24});
}

void Label::set_text(std::string text) {
  text_ = std::move(text);
  schedule_paint();
}

const std::string& Label::text() const {
  return text_;
}

void Label::set_color(render::skia::Color color) {
  color_ = color;
  has_color_ = true;
  schedule_paint();
}

void Label::clear_color() {
  has_color_ = false;
  schedule_paint();
}

void Label::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Rect& b = bounds();
  const std::wstring w = utf8_to_wide(text_);
  const render::skia::Color c =
      has_color_ ? color_ : Theme::current().text;
  canvas->draw_text(b.x + 4, b.y + 4, w.c_str(), c);
}

}  // namespace views
}  // namespace ui

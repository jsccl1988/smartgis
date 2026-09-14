// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/label.h"

#include "render/skia/canvas.h"
#include "ui/views/dpi.h"
#include "ui/views/theme.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {
namespace {

// Matches paint_self text inset (4px each side) at 96 DPI.
constexpr int kPadX = 8;
constexpr int kPadY = 8;
constexpr int kMinHeight = 24;

float scale_for(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

Size preferred_for_text(const std::string& text, float scale) {
  const Size ink = measure_text_utf8(text);
  const int pad_x = dip_to_px(kPadX, scale);
  const int pad_y = dip_to_px(kPadY, scale);
  const int min_h = dip_to_px(kMinHeight, scale);
  const int h = dip_to_px(ink.height, scale) + pad_y;
  return {dip_to_px(ink.width, scale) + pad_x, h > min_h ? h : min_h};
}

}  // namespace

Label::Label(std::string text) : text_(std::move(text)) {
  set_preferred_size(preferred_for_text(text_, 1.f));
}

void Label::set_text(std::string text) {
  text_ = std::move(text);
  set_preferred_size(preferred_for_text(text_, scale_for(this)));
  schedule_paint();
}

void Label::on_device_scale_factor_changed(float /*old_scale*/, float new_scale) {
  set_preferred_size(preferred_for_text(text_, new_scale));
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

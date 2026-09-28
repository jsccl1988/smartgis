// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/text/label.h"

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

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

}  // namespace

Label::Label(std::string text) : text_(std::move(text)) {
  rebuild_text_cache();
}

void Label::rebuild_text_cache() {
  const float scale = scale_for(this);
  wide_ = utf8_to_wide(text_);
  ink_ = text_.empty() ? Size{} : measure_text_utf8(text_, scale);
  ink_scale_ = scale;
  const int pad_x = dip_to_px(kPadX, scale);
  const int pad_y = dip_to_px(kPadY, scale);
  const int min_h = dip_to_px(kMinHeight, scale);
  const int h = ink_.height + pad_y;
  set_preferred_size({ink_.width + pad_x, h > min_h ? h : min_h});
  invalidate_commands();
}

void Label::set_text(std::string text) {
  if (text == text_) {
    return;
  }
  text_ = std::move(text);
  rebuild_text_cache();
  schedule_paint();
}

void Label::on_device_scale_factor_changed(float /*old_scale*/,
                                          float /*new_scale*/) {
  rebuild_text_cache();
}

const std::string& Label::text() const {
  return text_;
}

void Label::set_color(ui::gfx::Color color) {
  color_ = color;
  has_color_ = true;
  invalidate_commands();
  schedule_paint();
}

void Label::clear_color() {
  has_color_ = false;
  invalidate_commands();
  schedule_paint();
}

void Label::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Rect& b = bounds();
  const ui::gfx::Color c =
      has_color_ ? color_ : Theme::current().text;
  const float scale = scale_for(this);
  if (ink_scale_ != scale) {
    rebuild_text_cache();
  }
  const int ink_h = ink_.height > 0 ? ink_.height : dip_to_px(12, scale);
  const int pad_x = dip_to_px(4, scale);
  int text_y = b.y + pad_x;
  if (b.height > ink_h) {
    text_y = b.y + (b.height - ink_h) / 2;
  }
  if (!wide_.empty()) {
    canvas->draw_text(b.x + pad_x, text_y, wide_.c_str(), c);
  }
}


std::string_view Label::paint_role() const {
  return "label";
}
}  // namespace views
}  // namespace ui

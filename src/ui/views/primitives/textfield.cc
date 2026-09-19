// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/textfield.h"

#include "render/skia/canvas.h"
#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {

Textfield::Textfield() {
  set_preferred_size({200, 24});
  set_focusable(true);
}

void Textfield::set_text(std::string text) {
  if (text_ == text) {
    return;
  }
  text_ = std::move(text);
  schedule_paint();
}

const std::string& Textfield::text() const {
  return text_;
}

void Textfield::set_change(std::function<void()> fn) {
  change_ = std::move(fn);
}

void Textfield::notify_change() {
  schedule_paint();
  if (change_) {
    change_();
  }
}

bool Textfield::delete_last_char() {
  if (text_.empty()) {
    return true;
  }
  // Drop the last UTF-8 code point (including trailing continuation bytes).
  size_t i = text_.size();
  do {
    --i;
  } while (i > 0 && (static_cast<unsigned char>(text_[i]) & 0xC0) == 0x80);
  text_.resize(i);
  notify_change();
  return true;
}

bool Textfield::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kDown && e.button == 1) {
    request_focus();
    return true;
  }
  return false;
}

bool Textfield::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || !is_focused() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (e.vk == VK_BACK) {
    return delete_last_char();
  }
  return false;
}

bool Textfield::on_char_event(const CharEvent& e) {
  if (!is_enabled() || !is_focused()) {
    return false;
  }
  if (e.ch == L'\b') {
    return delete_last_char();
  }
  if (e.ch < 32 || e.ch == 127) {
    return false;
  }
  const wchar_t buf[2] = {e.ch, 0};
  text_ += wide_to_utf8(buf);
  notify_change();
  return true;
}

void Textfield::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height,
                    is_enabled() ? t.control_bg : t.control_disabled);
  std::wstring w = utf8_to_wide(text_);
  if (is_focused()) {
    w.push_back(L'|');
    draw_focus_ring(canvas, b);
  }
  canvas->draw_text(b.x + 4, b.y + 4, w.c_str(),
                    is_enabled() ? t.text : t.text_muted);
}

}  // namespace views
}  // namespace ui

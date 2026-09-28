// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/testing/harness/event_generator.h"

#include "ui/views/kernel/shell/event.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

EventGenerator::EventGenerator(Widget* widget) : widget_(widget) {}

bool EventGenerator::move_to(int x, int y) {
  last_x_ = x;
  last_y_ = y;
  if (!widget_ || !widget_->contents_view()) {
    return false;
  }
  MouseEvent e;
  e.type = MouseEvent::Type::kMove;
  e.x = x;
  e.y = y;
  widget_->send_mouse(e);
  return true;
}

bool EventGenerator::press(int button) {
  if (!widget_) {
    return false;
  }
  MouseEvent e;
  e.type = MouseEvent::Type::kDown;
  e.button = button;
  e.x = last_x_;
  e.y = last_y_;
  return widget_->send_mouse(e);
}

bool EventGenerator::release(int button) {
  if (!widget_) {
    return false;
  }
  MouseEvent e;
  e.type = MouseEvent::Type::kUp;
  e.button = button;
  e.x = last_x_;
  e.y = last_y_;
  return widget_->send_mouse(e);
}

bool EventGenerator::click(int x, int y, int button) {
  if (!move_to(x, y)) {
    return false;
  }
  if (!press(button)) {
    return false;
  }
  return release(button);
}

bool EventGenerator::drag(int x0, int y0, int x1, int y1, int button) {
  if (!move_to(x0, y0)) {
    return false;
  }
  if (!press(button)) {
    return false;
  }
  if (!move_to(x1, y1)) {
    return false;
  }
  return release(button);
}

bool EventGenerator::key_press(std::uint32_t vk) {
  if (!widget_) {
    return false;
  }
  KeyEvent down;
  down.type = KeyEvent::Type::kDown;
  down.vk = vk;
  KeyEvent up;
  up.type = KeyEvent::Type::kUp;
  up.vk = vk;
  const bool handled_down = widget_->send_key(down);
  const bool handled_up = widget_->send_key(up);
  return handled_down || handled_up;
}

bool EventGenerator::type_char(wchar_t ch) {
  if (!widget_) {
    return false;
  }
  CharEvent e;
  e.ch = ch;
  return widget_->send_char(e);
}

bool EventGenerator::type_utf8(std::string_view utf8) {
  const std::wstring wide = utf8_to_wide(std::string(utf8));
  bool ok = true;
  for (wchar_t ch : wide) {
    ok = type_char(ch) && ok;
  }
  return ok;
}

}  // namespace views
}  // namespace ui

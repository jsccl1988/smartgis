// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/menu_bar.h"

#include "render/skia/canvas.h"
#include "ui/views/dpi.h"
#include "ui/views/theme.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kBarHeightDip = 28;
constexpr int kItemPadXDip = 12;
constexpr int kItemPadYDip = 6;

float view_scale(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

}  // namespace

MenuBar::MenuBar() {
  set_preferred_size({200, dip_to_px(kBarHeightDip, 1.f)});
  set_focusable(true);
}

void MenuBar::add_item(std::string label, Invoke invoke) {
  items_.push_back({std::move(label), std::move(invoke)});
  schedule_paint();
}

void MenuBar::clear() {
  items_.clear();
  hover_ = -1;
  schedule_paint();
}

void MenuBar::on_device_scale_factor_changed(float /*old_scale*/,
                                           float new_scale) {
  set_preferred_size(
      {preferred_size().width, dip_to_px(kBarHeightDip, new_scale)});
}

int MenuBar::item_width(size_t i) const {
  if (i >= items_.size()) {
    return 0;
  }
  const float scale = view_scale(this);
  const Size ink = measure_text_utf8(items_[i].label);
  return dip_to_px(ink.width, scale) + dip_to_px(kItemPadXDip, scale) * 2;
}

Rect MenuBar::item_rect(size_t i) const {
  const Rect& b = bounds();
  int x = b.x;
  for (size_t n = 0; n < i && n < items_.size(); ++n) {
    x += item_width(n);
  }
  return {x, b.y, item_width(i), b.height};
}

int MenuBar::item_at(int x, int y) const {
  const Rect& b = bounds();
  if (!b.contains(x, y)) {
    return -1;
  }
  int cursor = b.x;
  for (size_t i = 0; i < items_.size(); ++i) {
    const int w = item_width(i);
    if (x >= cursor && x < cursor + w) {
      return static_cast<int>(i);
    }
    cursor += w;
  }
  return -1;
}

void MenuBar::activate(int i) {
  if (i < 0 || static_cast<size_t>(i) >= items_.size()) {
    return;
  }
  if (items_[static_cast<size_t>(i)].invoke) {
    items_[static_cast<size_t>(i)].invoke();
  }
}

bool MenuBar::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled()) {
    return false;
  }
  const int i = item_at(event.x, event.y);
  if (event.type == MouseEvent::Type::kMove) {
    if (hover_ != i) {
      hover_ = i;
      schedule_paint();
    }
    return i >= 0;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1 && i >= 0) {
    hover_ = i;
    activate(i);
    schedule_paint();
    return true;
  }
  if (event.type == MouseEvent::Type::kDown && event.button == 1 && i >= 0) {
    hover_ = i;
    schedule_paint();
    return true;
  }
  return View::on_mouse_event(event);
}

bool MenuBar::on_key_event(const KeyEvent& event) {
  if (!is_enabled() || event.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (event.vk == VK_MENU) {
    request_focus();
    if (hover_ < 0 && !items_.empty()) {
      hover_ = 0;
      schedule_paint();
    }
    return true;
  }
  if (!is_focused() || items_.empty()) {
    return false;
  }
  if (event.vk == VK_LEFT) {
    hover_ = (hover_ <= 0) ? static_cast<int>(items_.size()) - 1 : hover_ - 1;
    schedule_paint();
    return true;
  }
  if (event.vk == VK_RIGHT) {
    hover_ = (hover_ + 1) % static_cast<int>(items_.size());
    schedule_paint();
    return true;
  }
  if (event.vk == VK_SPACE || event.vk == VK_RETURN) {
    activate(hover_ >= 0 ? hover_ : 0);
    return true;
  }
  return false;
}

void MenuBar::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const float scale = view_scale(this);
  const int text_y = b.y + dip_to_px(kItemPadYDip, scale);
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_header);
  for (size_t i = 0; i < items_.size(); ++i) {
    const Rect r = item_rect(i);
    render::skia::Color fill = t.panel_header;
    if (static_cast<int>(i) == hover_) {
      fill = is_pressed() ? t.control_press : t.control_hover;
    }
    canvas->fill_rect(r.x, r.y, r.width, r.height, fill);
    // Clip label to the item cell so adjacent items never overpaint.
    canvas->save();
    if (r.width > 0 && r.height > 0) {
      canvas->clip_rect(r.x, r.y, r.width, r.height);
    }
    const std::wstring w = utf8_to_wide(items_[i].label);
    canvas->draw_text(r.x + dip_to_px(kItemPadXDip, scale), text_y, w.c_str(),
                      t.text_bright);
    canvas->restore();
  }
  if (is_focused()) {
    draw_focus_ring(canvas, b);
  }
}

}  // namespace views
}  // namespace ui

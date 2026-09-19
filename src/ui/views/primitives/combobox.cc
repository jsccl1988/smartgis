// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/combobox.h"

#include "render/skia/canvas.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/kernel/theme.h"
#include "ui/views/kernel/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kHeaderHeightDip = 24;
constexpr int kRowHeightDip = 22;

}  // namespace

class Combobox::ItemRow : public View {
 public:
  ItemRow(Combobox* owner, int index) : owner_(owner), index_(index) {
    const float scale = owner ? owner->scale_factor() : 1.f;
    set_preferred_size({dip_to_px(200, scale), dip_to_px(kRowHeightDip, scale)});
  }

  bool on_mouse_event(const MouseEvent& e) override {
    if (e.type == MouseEvent::Type::kUp && e.button == 1 && owner_) {
      owner_->select_item(index_);
      return true;
    }
    return e.type == MouseEvent::Type::kDown && e.button == 1;
  }

 protected:
  void paint_self(render::skia::Canvas* canvas) override {
    if (!canvas || !owner_ || index_ < 0 ||
        index_ >= static_cast<int>(owner_->items_.size())) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    const bool sel = owner_->selected_index() == index_;
    canvas->fill_rect(b.x, b.y, b.width, b.height,
                      sel ? t.accent : t.panel_header);
    const std::wstring w =
        utf8_to_wide(owner_->items_[static_cast<size_t>(index_)]);
    canvas->draw_text(b.x + 6, b.y + 3, w.c_str(), t.text_bright);
  }

 private:
  Combobox* owner_ = nullptr;
  int index_ = -1;
};

Combobox::Combobox() {
  set_preferred_size({200, kHeaderHeightDip});
  set_focusable(true);
}

float Combobox::scale_factor() const {
  if (widget()) {
    return widget()->device_scale_factor();
  }
  return 1.f;
}

int Combobox::header_height() const {
  return dip_to_px(kHeaderHeightDip, scale_factor());
}

int Combobox::row_height() const {
  return dip_to_px(kRowHeightDip, scale_factor());
}

void Combobox::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  set_preferred_size({preferred_size().width, header_height()});
  rebuild_rows();
}

void Combobox::add_item(std::string item) {
  items_.push_back(std::move(item));
  if (selected_ < 0) {
    selected_ = 0;
  }
  if (open_) {
    rebuild_rows();
  }
  schedule_paint();
}

void Combobox::set_selected_index(int i) {
  if (i < 0 || i >= static_cast<int>(items_.size())) {
    selected_ = -1;
    schedule_paint();
    return;
  }
  selected_ = i;
  schedule_paint();
}

int Combobox::selected_index() const {
  return selected_;
}

const std::string& Combobox::selected_text() const {
  if (selected_ < 0 || selected_ >= static_cast<int>(items_.size())) {
    return empty_;
  }
  return items_[static_cast<size_t>(selected_)];
}

void Combobox::set_change(std::function<void(int)> fn) {
  change_ = std::move(fn);
}

void Combobox::select_item(int index) {
  const int previous = selected_;
  set_selected_index(index);
  set_open(false);
  if (selected_ != previous && change_) {
    change_(selected_);
  }
}

void Combobox::set_open(bool open) {
  if (open_ == open) {
    return;
  }
  open_ = open;
  rebuild_rows();
  schedule_paint();
}

void Combobox::cycle(int delta) {
  if (items_.empty()) {
    return;
  }
  int next = selected_ + delta;
  if (next < 0) {
    next = static_cast<int>(items_.size()) - 1;
  } else if (next >= static_cast<int>(items_.size())) {
    next = 0;
  }
  if (next == selected_) {
    return;
  }
  selected_ = next;
  schedule_paint();
  if (change_) {
    change_(selected_);
  }
}

void Combobox::rebuild_rows() {
  const int hh = header_height();
  const int rh = row_height();
  const int extra = open_ ? static_cast<int>(items_.size()) * rh : 0;
  // Preferred stays header-sized so parent BoxLayout does not jump; open
  // rows expand the live bounds for hit-testing (dropdown overlay).
  set_preferred_size({preferred_size().width, hh});
  Rect b = bounds();
  if (b.width <= 0) {
    b.width = preferred_size().width;
  }
  b.height = hh + extra;
  set_bounds(b);

  for (size_t i = child_count(); i < items_.size(); ++i) {
    add_child(std::make_unique<ItemRow>(this, static_cast<int>(i)));
  }
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* c = child_at(i)) {
      c->set_visible(open_ && i < items_.size());
      if (c->is_visible()) {
        c->set_preferred_size({b.width, rh});
      }
    }
  }
  layout();
}

void Combobox::layout() {
  const Rect& b = bounds();
  const int hh = header_height();
  const int rh = row_height();
  for (size_t i = 0; i < child_count(); ++i) {
    View* c = child_at(i);
    if (!c || !c->is_visible()) {
      continue;
    }
    c->set_bounds({b.x, b.y + hh + static_cast<int>(i) * rh, b.width, rh});
  }
  View::layout();
}

bool Combobox::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (open_) {
    for (size_t i = 0; i < child_count(); ++i) {
      View* c = child_at(i);
      if (c && c->is_visible() && c->bounds().contains(e.x, e.y)) {
        return c->on_mouse_event(e);
      }
    }
  }
  const Rect header = {bounds().x, bounds().y, bounds().width, header_height()};
  if (!header.contains(e.x, e.y)) {
    return false;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    set_open(!open_);
    return true;
  }
  return e.type == MouseEvent::Type::kDown && e.button == 1;
}

bool Combobox::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (e.vk == VK_DOWN) {
    cycle(1);
    return true;
  }
  if (e.vk == VK_UP) {
    cycle(-1);
    return true;
  }
  if (e.vk == VK_SPACE || e.vk == VK_RETURN) {
    set_open(!open_);
    return true;
  }
  if (e.vk == VK_ESCAPE && open_) {
    set_open(false);
    return true;
  }
  return false;
}

void Combobox::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  render::skia::Color fill = t.control_fill;
  if (!is_enabled()) {
    fill = t.control_disabled;
  } else if (is_pressed() || is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(b.x, b.y, b.width, header_height(), fill);
  const std::wstring w = utf8_to_wide(selected_text());
  canvas->draw_text(b.x + 6, b.y + 4, w.c_str(),
                    is_enabled() ? t.text_bright : t.text_muted);
  canvas->draw_text(b.right() - 16, b.y + 4, open_ ? L"v" : L">", t.text_muted);
  if (is_focused()) {
    draw_focus_ring(canvas, {b.x, b.y, b.width, header_height()});
  }
}

}  // namespace views
}  // namespace ui

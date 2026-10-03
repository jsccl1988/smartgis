// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/input/combobox.h"

#include <algorithm>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kHeaderHeightDip = 28;
constexpr int kRowHeightDip = 26;
constexpr int kMaxVisibleRows = 8;

}  // namespace

// Floating list hosted in a kPopup Widget owned by Combobox.
class Combobox::DropdownList : public View {
 public:
  explicit DropdownList(Combobox* owner) : owner_(owner) {
    set_focusable(true);
  }

  void set_hover(int index) {
    if (hover_ == index) {
      return;
    }
    hover_ = index;
    schedule_paint();
  }

  int hover() const { return hover_; }

  bool on_mouse_event(const MouseEvent& e) override {
    if (!owner_) {
      return false;
    }
    const int row = row_at_y(e.y);
    if (e.type == MouseEvent::Type::kMove) {
      set_hover(row);
      return true;
    }
    if (e.type == MouseEvent::Type::kUp && e.button == 1 && row >= 0) {
      owner_->select_item(row);
      return true;
    }
    return e.type == MouseEvent::Type::kDown && e.button == 1;
  }

  bool on_key_event(const KeyEvent& e) override {
    if (!owner_ || e.type != KeyEvent::Type::kDown) {
      return false;
    }
    if (e.vk == VK_ESCAPE) {
      owner_->set_open(false);
      return true;
    }
    if (e.vk == VK_RETURN || e.vk == VK_SPACE) {
      const int pick = hover_ >= 0 ? hover_ : owner_->selected_index();
      if (pick >= 0) {
        owner_->select_item(pick);
      } else {
        owner_->set_open(false);
      }
      return true;
    }
    if (e.vk == VK_DOWN || e.vk == VK_UP) {
      const int n = owner_->item_count();
      if (n <= 0) {
        return true;
      }
      int next = hover_ >= 0 ? hover_ : owner_->selected_index();
      if (next < 0) {
        next = 0;
      } else {
        next += (e.vk == VK_DOWN) ? 1 : -1;
        if (next < 0) {
          next = n - 1;
        } else if (next >= n) {
          next = 0;
        }
      }
      set_hover(next);
      return true;
    }
    return false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas || !owner_) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
    canvas->stroke_rect(b.x, b.y, b.width, b.height, t.control_border, 1);

    const int rh = owner_->row_height();
    const int n = owner_->item_count();
    for (int i = 0; i < n; ++i) {
      const int y = b.y + i * rh;
      const bool sel = owner_->selected_index() == i;
      const bool hot = hover_ == i;
      ui::gfx::Color fill = t.panel_bg;
      if (sel) {
        fill = t.accent;
      } else if (hot) {
        fill = t.control_hover;
      } else if ((i % 2) != 0) {
        fill = t.row_alt;
      }
      canvas->fill_rect(b.x + 1, y, b.width - 2, rh, fill);
      const std::wstring w = utf8_to_wide(owner_->items_[static_cast<size_t>(i)]);
      const Size ink = measure_text_utf8(owner_->items_[static_cast<size_t>(i)],
                                         owner_->scale_factor());
      int text_y = y + 4;
      if (ink.height > 0 && rh > ink.height) {
        text_y = y + (rh - ink.height) / 2;
      }
      canvas->draw_text(b.x + 8, text_y, w.c_str(),
                        sel ? t.text_bright : t.text);
    }
  }

 private:
  int row_at_y(int y) const {
    if (!owner_ || owner_->item_count() <= 0) {
      return -1;
    }
    const int rh = owner_->row_height();
    if (rh <= 0) {
      return -1;
    }
    const int i = (y - bounds().y) / rh;
    if (i < 0 || i >= owner_->item_count()) {
      return -1;
    }
    return i;
  }

  Combobox* owner_ = nullptr;
  int hover_ = -1;
};

Combobox::Combobox() {
  set_preferred_size({200, kHeaderHeightDip});
  set_focusable(true);
}

Combobox::~Combobox() {
  hide_popup();
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
  if (open_) {
    // Rebuild popup at the new scale.
    hide_popup();
    show_popup();
  }
}

void Combobox::add_item(std::string item) {
  items_.push_back(std::move(item));
  if (selected_ < 0) {
    selected_ = 0;
  }
  schedule_paint();
  if (open_) {
    hide_popup();
    show_popup();
  }
}

void Combobox::clear_items() {
  items_.clear();
  selected_ = -1;
  set_open(false);
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
  flush_closed_popup();
  if (open_ == open) {
    return;
  }
  if (open) {
    show_popup();
  } else {
    hide_popup();
  }
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

void Combobox::flush_closed_popup() {
  // Deactivate path DestroyWindows the HWND but leaves the C++ Widget in
  // |popup_| (will_close must not reset during request_close). Drop the
  // husk once the native window is gone.
  if (popup_ && !popup_->hwnd()) {
    popup_.reset();
  }
}

void Combobox::show_popup() {
  flush_closed_popup();
  if (items_.empty()) {
    open_ = false;
    return;
  }
  // Headless / unit tests: no host HWND — still track open_ for keyboard
  // and is_open() assertions without creating a floating list Widget.
  if (!widget() || !widget()->hwnd()) {
    open_ = true;
    schedule_paint();
    return;
  }
  hide_popup();

  const int visible = std::min(static_cast<int>(items_.size()), kMaxVisibleRows);
  const int rh = row_height();
  const int list_h = visible * rh;
  const int list_w = std::max(bounds().width, preferred_size().width);

  POINT origin = {bounds().x, bounds().y + header_height()};
  ClientToScreen(widget()->hwnd(), &origin);

  auto list = std::make_unique<DropdownList>(this);
  list->set_preferred_size({list_w, list_h});
  list->set_hover(selected_ >= 0 ? selected_ : 0);
  DropdownList* list_ptr = list.get();

  popup_ = std::make_unique<Widget>();
  Widget::InitParams params;
  params.title = L"";
  params.width = list_w;
  params.height = list_h;
  params.size_in_dips = false;  // already device pixels
  params.owner = widget()->hwnd();
  params.frame_kind = Widget::FrameKind::kPopup;
  params.has_screen_origin = true;
  params.screen_x = origin.x;
  params.screen_y = origin.y;
  params.dismiss_on_deactivate = true;
  if (!popup_->init(params)) {
    popup_.reset();
    open_ = false;
    return;
  }
  popup_->set_will_close([this] {
    open_ = false;
    schedule_paint();
    if (widget()) {
      request_focus();
    }
    // Do not reset |popup_| here — we are inside Widget::request_close.
  });
  popup_->set_contents_view(std::move(list));
  open_ = true;
  popup_->show();
  list_ptr->request_focus();
  schedule_paint();
}

void Combobox::hide_popup() {
  open_ = false;
  if (!popup_) {
    return;
  }
  std::unique_ptr<Widget> doomed = std::move(popup_);
  doomed->set_will_close({});
  if (doomed->hwnd()) {
    doomed->request_close();
  }
  doomed.reset();
  schedule_paint();
}

bool Combobox::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
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
  if (open_) {
    // Keys are handled by the popup list while it holds focus.
    if (e.vk == VK_ESCAPE) {
      set_open(false);
      return true;
    }
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
    set_open(true);
    return true;
  }
  return false;
}

void Combobox::paint_self(ui::gfx::Canvas* canvas) {
  flush_closed_popup();
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const int hh = header_height();
  ui::gfx::Color fill = t.control_fill;
  if (!is_enabled()) {
    fill = t.control_disabled;
  } else if (is_pressed() || open_) {
    fill = t.control_press;
  } else if (is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(b.x, b.y, b.width, hh, fill);

  ui::gfx::Color edge = t.control_border;
  if (is_enabled() && is_focused()) {
    edge = t.accent;
  } else if (is_enabled() && is_hovered()) {
    edge = t.control_hover;
  }
  canvas->stroke_rect(b.x, b.y, b.width, hh, edge, 1);

  const std::wstring w = utf8_to_wide(selected_text());
  const Size ink = measure_text_utf8(selected_text(), scale_factor());
  int text_y = b.y + 4;
  if (ink.height > 0 && hh > ink.height) {
    text_y = b.y + (hh - ink.height) / 2;
  }
  canvas->draw_text(b.x + 8, text_y, w.c_str(),
                    is_enabled() ? t.text_bright : t.text_muted);
  canvas->draw_text(b.right() - 18, text_y, open_ ? L"\u25B2" : L"\u25BC",
                    t.text_muted);
  if (is_focused()) {
    draw_focus_ring(canvas, {b.x, b.y, b.width, hh});
  }
}

std::string_view Combobox::paint_role() const {
  return "combobox";
}
}  // namespace views
}  // namespace ui

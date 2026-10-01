// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/collection/tab_strip.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kTabHeightDip = 32;
constexpr int kTabPadXDip = 12;
constexpr int kTabMinWidthDip = 56;

float view_scale(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

}  // namespace

TabStrip::TabStrip() {
  set_preferred_size({400, 280});
}

int TabStrip::tab_height() const {
  return dip_to_px(kTabHeightDip, view_scale(this));
}

Rect TabStrip::header_bounds() const {
  const Rect& b = bounds();
  return {b.x, b.y, b.width, tab_height()};
}

void TabStrip::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  layout();
}

int TabStrip::add_tab(std::string title, std::unique_ptr<View> page) {
  View* raw = page.get();
  add_child(std::move(page));
  titles_.push_back(std::move(title));
  pages_.push_back(raw);
  if (active_ < 0) {
    active_ = 0;
  }
  apply_page_visibility();
  return static_cast<int>(pages_.size()) - 1;
}

void TabStrip::set_active(int i) {
  if (i < 0 || i >= static_cast<int>(pages_.size())) {
    return;
  }
  active_ = i;
  apply_page_visibility();
  schedule_paint();
}

int TabStrip::active() const {
  return active_;
}

View* TabStrip::page_at(int i) const {
  if (i < 0 || i >= static_cast<int>(pages_.size())) {
    return nullptr;
  }
  return pages_[static_cast<size_t>(i)];
}

int TabStrip::tab_count() const {
  return static_cast<int>(pages_.size());
}

void TabStrip::set_change(std::function<void(int)> fn) {
  change_ = std::move(fn);
}

int TabStrip::tab_width_at(int i) const {
  if (i < 0 || i >= static_cast<int>(titles_.size())) {
    return 0;
  }
  const float scale = view_scale(this);
  // measure_text_utf8(..., scale) already returns device pixels.
  const Size text =
      measure_text_utf8(titles_[static_cast<size_t>(i)], scale);
  const int pad = dip_to_px(kTabPadXDip, scale) * 2;
  const int min_w = dip_to_px(kTabMinWidthDip, scale);
  return std::max(min_w, text.width + pad);
}

int TabStrip::tab_x_at(int i) const {
  const Rect& b = bounds();
  int x = b.x;
  for (int j = 0; j < i && j < static_cast<int>(titles_.size()); ++j) {
    x += tab_width_at(j);
  }
  return x;
}

void TabStrip::apply_page_visibility() {
  const Rect& b = bounds();
  const int th = tab_height();
  constexpr int kHeaderGapPx = 2;
  const int body_top = b.y + th + kHeaderGapPx;
  const int body_h =
      b.height > (th + kHeaderGapPx) ? b.height - th - kHeaderGapPx : 0;
  const Rect page_bounds = {b.x, body_top, b.width, body_h};
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    View* page = pages_[static_cast<size_t>(i)];
    if (!page) {
      continue;
    }
    page->set_bounds(page_bounds);
    page->set_visible(i == active_);
  }
}

void TabStrip::layout() {
  apply_page_visibility();
  View::layout();
}

int TabStrip::tab_at(int x, int y) const {
  const Rect& b = bounds();
  if (pages_.empty() || y < b.y || y >= b.y + tab_height() || x < b.x ||
      x >= b.right()) {
    return -1;
  }
  int cursor = b.x;
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    const int w = tab_width_at(i);
    if (x >= cursor && x < cursor + w) {
      return i;
    }
    cursor += w;
  }
  return -1;
}

bool TabStrip::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    const int i = tab_at(e.x, e.y);
    if (i >= 0) {
      const int previous = active_;
      set_active(i);
      if (i != previous && change_) {
        change_(i);
      }
      return true;
    }
  }
  if (e.type == MouseEvent::Type::kDown && e.button == 1 &&
      tab_at(e.x, e.y) >= 0) {
    return true;
  }
  return View::on_mouse_event(e);
}

void TabStrip::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const float scale = view_scale(this);
  const int th = tab_height();
  canvas->fill_rect(b.x, b.y, b.width, th, t.panel_header);
  if (pages_.empty()) {
    return;
  }
  // Active labels sit on accent — never theme text_bright (light pack is
  // near-black and fails contrast on #007acc).
  const ui::gfx::Color accent_label = ui::gfx::color_rgb(255, 255, 255);
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    const int x = tab_x_at(i);
    const int w = tab_width_at(i);
    const bool on = (i == active_);
    if (on) {
      canvas->fill_rect(x, b.y, w, th, t.accent);
    }
    canvas->save();
    canvas->clip_rect(x, b.y, w, th);
    const std::string& title_u8 = titles_[static_cast<size_t>(i)];
    const Size text = measure_text_utf8(title_u8, scale);
    const int text_x = x + dip_to_px(kTabPadXDip, scale);
    const int text_y = b.y + std::max(0, (th - text.height) / 2);
    canvas->draw_text(text_x, text_y, utf8_to_wide(title_u8).c_str(),
                      on ? accent_label : t.text);
    canvas->restore();
  }
}

std::string_view TabStrip::paint_role() const {
  return "tab_strip";
}

}  // namespace views
}  // namespace ui

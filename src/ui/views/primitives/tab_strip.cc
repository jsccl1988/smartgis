// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/tab_strip.h"

#include "render/skia/canvas.h"
#include "ui/views/dpi.h"
#include "ui/views/theme.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kTabHeightDip = 28;
constexpr int kTabPadXDip = 10;
constexpr int kTabPadYDip = 6;

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
  if (i < 0 || i >= static_cast<int>(pages_.size()) || i == active_) {
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

void TabStrip::apply_page_visibility() {
  const Rect& b = bounds();
  const int th = tab_height();
  const int body_h = b.height > th ? b.height - th : 0;
  const Rect page_bounds = {b.x, b.y + th, b.width, body_h};
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    View* page = pages_[static_cast<size_t>(i)];
    if (!page) {
      continue;
    }
    const bool on = (i == active_);
    // Keep inactive pages sized so attach/resize still has a real client rect.
    page->set_bounds(page_bounds);
    page->set_visible(on);
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
  const int w = b.width / static_cast<int>(pages_.size());
  if (w <= 0) {
    return -1;
  }
  int i = (x - b.x) / w;
  if (i < 0) {
    i = 0;
  }
  if (i >= static_cast<int>(pages_.size())) {
    i = static_cast<int>(pages_.size()) - 1;
  }
  return i;
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

void TabStrip::paint_self(render::skia::Canvas* canvas) {
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
  const int w = b.width / static_cast<int>(pages_.size());
  const int text_x = dip_to_px(kTabPadXDip, scale);
  const int text_y = b.y + dip_to_px(kTabPadYDip, scale);
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    const int x = b.x + i * w;
    if (i == active_) {
      canvas->fill_rect(x, b.y, w, th, t.accent);
    }
    // Clip label to the tab cell so long titles cannot paint into neighbors.
    canvas->save();
    canvas->clip_rect(x, b.y, w, th);
    const std::wstring title = utf8_to_wide(titles_[static_cast<size_t>(i)]);
    canvas->draw_text(x + text_x, text_y, title.c_str(), t.text_bright);
    canvas->restore();
  }
}

}  // namespace views
}  // namespace ui

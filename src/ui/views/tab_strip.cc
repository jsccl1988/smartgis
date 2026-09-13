// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/tab_strip.h"

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

TabStrip::TabStrip() {
  set_preferred_size({400, 280});
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
  const int body_h = b.height > tab_height() ? b.height - tab_height() : 0;
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    View* page = pages_[static_cast<size_t>(i)];
    if (!page) {
      continue;
    }
    const bool on = (i == active_);
    page->set_visible(on);
    if (on) {
      page->set_bounds({b.x, b.y + tab_height(), b.width, body_h});
    }
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
  canvas->fill_rect(b.x, b.y, b.width, tab_height(), t.panel_header);
  if (pages_.empty()) {
    return;
  }
  const int w = b.width / static_cast<int>(pages_.size());
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    const int x = b.x + i * w;
    if (i == active_) {
      canvas->fill_rect(x, b.y, w, tab_height(), t.accent);
    }
    const std::wstring title = utf8_to_wide(titles_[static_cast<size_t>(i)]);
    canvas->draw_text(x + 8, b.y + 6, title.c_str(), t.text_bright);
  }
}

}  // namespace views
}  // namespace ui

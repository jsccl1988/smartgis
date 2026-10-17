// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/collection/tab_strip.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/menu/context_menu.h"

namespace ui {
namespace views {
namespace {

constexpr int kTabHeightDip = 26;
constexpr int kTabPadXDip = 12;
constexpr int kTabMinWidthDip = 56;
constexpr int kTabOverflowDip = 28;
constexpr int kOverflowHit = -2;

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
  const int th = tab_height();
  if (header_placement_ == HeaderPlacement::kBottom) {
    const int y = b.height > th ? b.y + b.height - th : b.y;
    return {b.x, y, b.width, th};
  }
  return {b.x, b.y, b.width, th};
}

void TabStrip::set_header_placement(HeaderPlacement placement) {
  if (header_placement_ == placement) {
    return;
  }
  header_placement_ = placement;
  apply_page_visibility();
  schedule_paint();
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
  const int index = static_cast<int>(pages_.size()) - 1;
  if (index >= 0 && index < kMaxTabs) {
    included_[index] = 1;
  }
  if (active_ < 0) {
    active_ = 0;
  }
  apply_page_visibility();
  return static_cast<int>(pages_.size()) - 1;
}

bool TabStrip::replace_page(int i, std::unique_ptr<View> page) {
  if (i < 0 || i >= static_cast<int>(pages_.size()) || !page) {
    return false;
  }
  View* old = pages_[static_cast<size_t>(i)];
  View* raw = page.get();
  add_child(std::move(page));
  pages_[static_cast<size_t>(i)] = raw;
  if (old) {
    remove_child(old);
  }
  apply_page_visibility();
  // apply may hide this page (not the active tab). Layout anyway so the first
  // show is not an empty box: set_visible later sees unchanged bounds.
  if (raw->bounds().width > 0 && raw->bounds().height > 0) {
    raw->layout();
  }
  schedule_paint();
  return true;
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

int TabStrip::natural_tab_width_at(int i) const {
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

void TabStrip::set_tab_included(int i, bool included) {
  if (i < 0 || i >= static_cast<int>(titles_.size())) {
    return;
  }
  if (i >= kMaxTabs) {
    return;
  }
  const uint8_t next = included ? 1 : 0;
  if (included_[i] == next) {
    return;
  }
  included_[i] = next;
  schedule_paint();
}

bool TabStrip::tab_included(int i) const {
  if (i < 0 || i >= static_cast<int>(titles_.size())) {
    return false;
  }
  if (i >= kMaxTabs) {
    return true;
  }
  return included_[i] != 0;
}

bool TabStrip::has_tab_overflow() const {
  SlotLayout slots;
  layout_slots(&slots);
  return slots.chevron_w > 0;
}

int TabStrip::overflow_count() const {
  SlotLayout slots;
  layout_slots(&slots);
  return slots.overflow_n;
}

int TabStrip::overflow_tab_at(int i) const {
  SlotLayout slots;
  layout_slots(&slots);
  if (i < 0 || i >= slots.overflow_n) {
    return -1;
  }
  return slots.overflow[i];
}

void TabStrip::layout_slots(SlotLayout* out) const {
  SlotLayout slots;
  const int n = std::min(static_cast<int>(titles_.size()), kMaxTabs);
  int included_buf[kMaxTabs];
  int included_n = 0;
  for (int i = 0; i < n; ++i) {
    if (tab_included(i)) {
      included_buf[included_n++] = i;
    }
  }
  const int avail = header_bounds().width;
  if (included_n == 0 || avail <= 0) {
    *out = slots;
  return;
  }
  int total = 0;
  for (int p = 0; p < included_n; ++p) {
    total += natural_tab_width_at(included_buf[p]);
  }
  const float scale = view_scale(this);
  const int floor_w = std::max(1, dip_to_px(kTabMinWidthDip, scale));
  // 1) natural widths fit
  // 2) mild overflow: shrink, but not below the readable floor
  // 3) floor still overflows: keep a window that includes the active tab and
  //    list the rest in the overflow menu (every included tab stays reachable)
  if (total <= avail) {
    for (int p = 0; p < included_n; ++p) {
      const int index = included_buf[p];
      slots.width[index] = natural_tab_width_at(index);
    }
    *out = slots;
  return;
  }
  const int count = included_n;
  if (count == 1 || floor_w * count <= avail) {
    if (count == 1) {
      const int index = included_buf[0];
      const int natural = natural_tab_width_at(index);
      slots.width[index] = std::min(avail, natural);
      *out = slots;
  return;
    }
    for (int p = 0; p < included_n; ++p) {
      const int index = included_buf[p];
      const int natural = natural_tab_width_at(index);
      const int scaled = std::max(1, (natural * avail) / std::max(1, total));
      slots.width[index] = std::max(floor_w, scaled);
    }
    *out = slots;
  return;
  }
  const int chevron = std::max(1, dip_to_px(kTabOverflowDip, scale));
  slots.chevron_w = std::min(chevron, std::max(1, avail / 4));
  const int body = std::max(1, avail - slots.chevron_w);
  int max_k = std::max(1, body / floor_w);
  if (max_k > count) {
    max_k = count;
  }
  while (max_k > 1 && floor_w * max_k > body) {
    --max_k;
  }
  int active_pos = 0;
  bool active_in = false;
  for (int p = 0; p < count; ++p) {
    if (included_buf[p] == active_) {
      active_pos = p;
      active_in = true;
      break;
    }
  }
  int start = 0;
  if (active_in) {
    start = active_pos - max_k + 1;
    if (start < 0) {
      start = 0;
    }
  }
  if (start > count - max_k) {
    start = count - max_k;
  }
  if (start < 0) {
    start = 0;
  }
  const int base = body / max_k;
  const int rem = body - base * max_k;
  for (int p = 0; p < max_k; ++p) {
    const int index = included_buf[start + p];
    slots.width[index] = base + (p < rem ? 1 : 0);
  }
  for (int p = 0; p < count; ++p) {
    if (p < start || p >= start + max_k) {
      if (slots.overflow_n < kMaxTabs) {
        slots.overflow[slots.overflow_n++] = included_buf[p];
      }
    }
  }
  *out = slots;
  return;
}

int TabStrip::tab_width_at(int i) const {
  if (i < 0 || i >= kMaxTabs) {
    return 0;
  }
  SlotLayout slots;
  layout_slots(&slots);
  return slots.width[i];
}

int TabStrip::tab_x_at(int i) const {
  SlotLayout slots;
  layout_slots(&slots);
  int x = bounds().x;
  const int n = std::min(i, kMaxTabs);
  for (int j = 0; j < n; ++j) {
    x += slots.width[j];
  }
  return x;
}

void TabStrip::apply_page_visibility() {
  const Rect& b = bounds();
  const int th = tab_height();
  constexpr int kHeaderGapPx = 2;
  const int body_h =
      b.height > (th + kHeaderGapPx) ? b.height - th - kHeaderGapPx : 0;
  const int body_top = (header_placement_ == HeaderPlacement::kBottom)
                           ? b.y
                           : b.y + th + kHeaderGapPx;
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
  const Rect header = header_bounds();
  if (pages_.empty() || y < header.y || y >= header.y + header.height ||
      x < header.x || x >= header.x + header.width) {
    return -1;
  }
  SlotLayout slots;
  layout_slots(&slots);
  if (slots.chevron_w > 0 &&
      x >= header.x + header.width - slots.chevron_w) {
    return kOverflowHit;
  }
  int cursor = header.x;
  for (int i = 0; i < kMaxTabs; ++i) {
    const int w = slots.width[i];
    if (w > 0 && x >= cursor && x < cursor + w) {
      return i;
    }
    cursor += w;
  }
  return -1;
}

void TabStrip::open_overflow_menu() {
  SlotLayout slots;
  layout_slots(&slots);
  if (slots.overflow_n <= 0) {
    return;
  }
  std::vector<MenuItem> items;
  items.reserve(static_cast<size_t>(slots.overflow_n));
  for (int n = 0; n < slots.overflow_n; ++n) {
    const int index = slots.overflow[n];
    MenuItem item;
    item.label = titles_[static_cast<size_t>(index)];
    item.invoke = [this, index]() {
      if (index == active_) {
        return;
      }
      if (change_) {
        change_(index);
      }
      if (active_ != index) {
        set_active(index);
      }
    };
    items.push_back(std::move(item));
  }
  const Rect header = header_bounds();
  Point screen{header.x + header.width - slots.chevron_w,
               header.y + header.height};
  HWND owner = widget() ? widget()->hwnd() : nullptr;
  if (owner) {
    POINT pt{screen.x, screen.y};
    if (ClientToScreen(owner, &pt)) {
      screen.x = pt.x;
      screen.y = pt.y;
    }
  }
  show_context_menu(owner, screen, items);
}

bool TabStrip::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  // Activate on press (not only release): OS SendInput / DXGI present focus
  // races often drop the matching mouse-up. Call |change_| before set_active
  // so switch_map_tab does not early-return on an already-flipped active_
  // (plain-launch visual_review #6). If |change_| does not update chrome
  // (unit tests / stubs), fall back to set_active.
  if (e.type == MouseEvent::Type::kDown && e.button == 1) {
    const int i = tab_at(e.x, e.y);
    if (i == kOverflowHit) {
      open_overflow_menu();
      return true;
    }
    if (i >= 0) {
      if (i != active_) {
        if (change_) {
          change_(i);
        }
        if (active_ != i) {
          set_active(i);
        }
      }
      return true;
    }
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1 &&
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
  const Rect header = header_bounds();
  const float scale = view_scale(this);
  const int th = header.height;
  canvas->fill_rect(header.x, header.y, header.width, th, t.panel_header);
  // Hairline toward the page body — muted border, not a full-width accent
  // (accent on the whole header looked like dual active tabs across strips).
  if (header_placement_ == HeaderPlacement::kBottom) {
    canvas->fill_rect(header.x, header.y, header.width, 1, t.control_border);
  } else {
    canvas->fill_rect(header.x, header.y + th - 1, header.width, 1,
                      t.control_border);
  }
  if (pages_.empty()) {
    return;
  }
  SlotLayout slots;
  layout_slots(&slots);
  // Subtler active tab: muted plate + accent edge (not a flat neon slab).
  // Accent bar keeps ui_shell_dark score samples while matching QGIS horizon.
  const ui::gfx::Color accent_label = ui::gfx::color_rgb(255, 255, 255);
  const int accent_bar = std::max(2, dip_to_px(3, scale));
  int cursor = header.x;
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    const int w = i < kMaxTabs ? slots.width[i] : 0;
    const int x = cursor;
    cursor += w;
    if (w <= 0) {
      continue;
    }
    const bool on = (i == active_);
    if (on) {
      canvas->fill_rect(x, header.y, w, th, t.control_press);
      if (header_placement_ == HeaderPlacement::kBottom) {
        canvas->fill_rect(x, header.y, w, accent_bar, t.accent);
      } else {
        canvas->fill_rect(x, header.y + th - accent_bar, w, accent_bar,
                          t.accent);
      }
    } else {
      canvas->fill_rect(x, header.y, w, th, t.panel_header);
    }
    canvas->save();
    canvas->clip_rect(x, header.y, w, th);
    const std::string& title_u8 = titles_[static_cast<size_t>(i)];
    const Size text = measure_text_utf8(title_u8, scale);
    const int pad_x = dip_to_px(kTabPadXDip, scale);
    // Center the glyph box in the accent cell (avoids bottom-heavy labels).
    const int text_x = x + std::max(pad_x, (w - text.width) / 2);
    const int text_y = header.y + std::max(0, (th - text.height) / 2);
    canvas->draw_text(text_x, text_y, utf8_to_wide(title_u8).c_str(),
                      on ? accent_label : t.text_bright);
    canvas->restore();
  }
  if (slots.chevron_w > 0) {
    const int x = header.x + header.width - slots.chevron_w;
    canvas->fill_rect(x, header.y, slots.chevron_w, th, t.panel_header);
    canvas->fill_rect(x, header.y, 1, th, t.control_border);
    const char* mark = "...";
    const Size text = measure_text_utf8(mark, scale);
    const int text_x = x + std::max(0, (slots.chevron_w - text.width) / 2);
    const int text_y = header.y + std::max(0, (th - text.height) / 2);
    canvas->draw_text(text_x, text_y, utf8_to_wide(mark).c_str(), t.text_bright);
  }
}

std::string_view TabStrip::paint_role() const {
  return "tab_strip";
}

}  // namespace views
}  // namespace ui

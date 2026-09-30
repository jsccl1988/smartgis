// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout/splitter.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/layout/view_traits.h"

namespace ui {
namespace views {
namespace {

static_assert(static_cast<int>(Splitter::Orientation::kHorizontal) ==
              static_cast<int>(Axis::kHorizontal));
static_assert(static_cast<int>(Splitter::Orientation::kVertical) ==
              static_cast<int>(Axis::kVertical));

template <Axis A>
Rect split_bar(const Rect& host, int primary, int bar_px) {
  return axis_traits<A>::bar(host, primary, bar_px);
}

template <Axis A>
void place_panes(View* primary, View* secondary, const Rect& host, int primary_px,
                 int bar_px) {
  if (primary) {
    primary->set_bounds(axis_traits<A>::primary_pane(host, primary_px));
  }
  if (secondary) {
    secondary->set_bounds(
        axis_traits<A>::secondary_pane(host, primary_px, bar_px));
  }
}

}  // namespace

Splitter::Splitter(Orientation orientation) : orientation_(orientation) {
  set_preferred_size({200, 200});
}

void Splitter::set_collapsed(bool collapsed) {
  if (collapsed_ == collapsed) {
    return;
  }
  if (collapsed) {
    saved_primary_ = primary_extent_;
  } else if (saved_primary_ > 0) {
    primary_extent_ = saved_primary_;
  } else {
    // Collapsed before the first real layout 鈥?re-seed from preferred sizes.
    split_seeded_ = false;
  }
  collapsed_ = collapsed;
  layout();
  schedule_paint();
}

void Splitter::reseed() {
  split_seeded_ = false;
  layout();
  schedule_paint();
}

int Splitter::main_extent() const {
  const Rect& b = bounds();
  return is_horizontal() ? axis_traits<Axis::kHorizontal>::main(b)
                         : axis_traits<Axis::kVertical>::main(b);
}

Rect Splitter::bar_rect() const {
  const Rect& b = bounds();
  return is_horizontal() ? split_bar<Axis::kHorizontal>(b, primary_extent_, kBarPx)
                         : split_bar<Axis::kVertical>(b, primary_extent_, kBarPx);
}

void Splitter::seed_split_if_needed() {
  if (split_seeded_ || child_count() < 2) {
    return;
  }
  // layout() can run before the host has a real size; do not lock a 0 split.
  if (main_extent() <= kBarPx) {
    return;
  }
  View* a = child_at(0);
  View* b = child_at(1);
  // Prefer the stored preferred_size hint when it is explicitly 0 on the
  // secondary axis so a collapsed DiagnosticToolsPanel (preferred {0,0} but
  // BoxLayout get_preferred_size still ~200) does not steal map/3D height.
  const Size sa_hint = a->preferred_size();
  const Size sb_hint = b->preferred_size();
  const Size sa_layout = a->get_preferred_size();
  const Size sb_layout = b->get_preferred_size();
  const int pa_hint = is_horizontal() ? axis_traits<Axis::kHorizontal>::main(sa_hint)
                                      : axis_traits<Axis::kVertical>::main(sa_hint);
  const int pb_hint = is_horizontal() ? axis_traits<Axis::kHorizontal>::main(sb_hint)
                                      : axis_traits<Axis::kVertical>::main(sb_hint);
  const Size sa = (pa_hint <= 0) ? sa_hint : sa_layout;
  const Size sb = (pb_hint <= 0) ? sb_hint : sb_layout;
  const int pa = is_horizontal() ? axis_traits<Axis::kHorizontal>::main(sa)
                                 : axis_traits<Axis::kVertical>::main(sa);
  const int pb = is_horizontal() ? axis_traits<Axis::kHorizontal>::main(sb)
                                 : axis_traits<Axis::kVertical>::main(sb);
  const int inner = std::max(0, main_extent() - kBarPx);
  fixed_secondary_px_ = 0;
  if (pa <= 0 && pb <= 0) {
    // Both flex / hidden: primary keeps the work area; secondary stays at 0
    // until preferred size or a user drag grows it (Diagnostic Tools starts
    // at preferred 0 鈥?must not seed a 50/50 split that starves the map).
    primary_extent_ = inner;
    fixed_secondary_px_ = 0;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  } else if (pa <= 0) {
    // BrowserView pattern: flexible map/work pane + fixed ambox/inspector.
    primary_extent_ = inner - pb;
    fixed_secondary_px_ = pb;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  } else if (pb <= 0) {
    // Catalog (fixed preferred) + map tabs (flex).
    primary_extent_ = pa;
    resize_policy_ = ResizePolicy::kPrimaryFixed;
  } else {
    primary_extent_ = inner * pa / (pa + pb);
    resize_policy_ = ResizePolicy::kProportional;
  }
  split_seeded_ = true;
  last_main_ = main_extent();
}

void Splitter::adjust_for_host_resize() {
  const int main = main_extent();
  if (!split_seeded_ || collapsed_ || dragging_ || main <= kBarPx) {
    if (main > kBarPx) {
      last_main_ = main;
    }
    return;
  }
  if (last_main_ <= kBarPx) {
    last_main_ = main;
    return;
  }
  if (main == last_main_) {
    return;
  }

  const int inner = std::max(0, main - kBarPx);
  const int last_inner = std::max(0, last_main_ - kBarPx);
  if (user_adjusted_ || resize_policy_ == ResizePolicy::kProportional) {
    if (last_inner > 0) {
      primary_extent_ = inner * primary_extent_ / last_inner;
    }
  } else if (resize_policy_ == ResizePolicy::kSecondaryFixed) {
    primary_extent_ = inner - fixed_secondary_px_;
  }
  // kPrimaryFixed: keep primary_extent_; secondary absorbs growth.
  last_main_ = main;
}

void Splitter::clamp_primary() {
  const int main = main_extent();
  if (collapsed_) {
    primary_extent_ = std::max(0, main - kBarPx);
    return;
  }
  // Preference-0 secondary (DiagnosticToolsPanel collapsed): keep secondary at
  // 0px. Forcing kMinPanePx here left a ~40px chrome strip that painted over
  // the status bar (Diagnostic Tools / tab bleed).
  if (resize_policy_ == ResizePolicy::kSecondaryFixed &&
      fixed_secondary_px_ <= 0) {
    primary_extent_ = std::max(0, main - kBarPx);
    return;
  }
  const int max_primary = main - kBarPx - kMinPanePx;
  if (main < kMinPanePx * 2 + kBarPx) {
    primary_extent_ = std::max(0, std::min(primary_extent_, main - kBarPx));
    return;
  }
  primary_extent_ = std::max(kMinPanePx, std::min(primary_extent_, max_primary));
}

void Splitter::apply_child_bounds() {
  const Rect& b = bounds();
  View* a = child_count() > 0 ? child_at(0) : nullptr;
  View* sec = child_count() > 1 ? child_at(1) : nullptr;
  if (is_horizontal()) {
    place_panes<Axis::kHorizontal>(a, sec, b, primary_extent_, kBarPx);
  } else {
    place_panes<Axis::kVertical>(a, sec, b, primary_extent_, kBarPx);
  }
}

void Splitter::layout() {
  seed_split_if_needed();
  adjust_for_host_resize();
  clamp_primary();
  apply_child_bounds();
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child && child->is_visible()) {
      child->layout();
      child->sync_native_bounds();
    }
  }
}

void Splitter::begin_drag(int pos) {
  dragging_ = true;
  user_adjusted_ = true;
  drag_origin_ = pos;
  drag_primary_origin_ = primary_extent_;
  if (collapsed_) {
    collapsed_ = false;
    saved_primary_ = 0;
  }
}

void Splitter::update_drag(int pos) {
  primary_extent_ = drag_primary_origin_ + (pos - drag_origin_);
  clamp_primary();
  apply_child_bounds();
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child && child->is_visible()) {
      child->layout();
      child->sync_native_bounds();
    }
  }
  schedule_paint();
}

bool Splitter::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled() || !is_visible()) {
    return false;
  }
  const int pos = is_horizontal() ? event.x : event.y;
  if (dragging_) {
    if (event.type == MouseEvent::Type::kMove ||
        event.type == MouseEvent::Type::kUp) {
      update_drag(pos);
      if (event.type == MouseEvent::Type::kUp) {
        dragging_ = false;
      }
      return true;
    }
  }
  if (bar_rect().contains(event.x, event.y)) {
    SetCursor(LoadCursor(nullptr,
                         is_horizontal() ? IDC_SIZEWE : IDC_SIZENS));
    if (event.type == MouseEvent::Type::kDown && event.button == 1) {
      begin_drag(pos);
      return true;
    }
    return event.type == MouseEvent::Type::kMove ||
           event.type == MouseEvent::Type::kDown;
  }
  return View::on_mouse_event(event);
}

void Splitter::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  const Rect bar = bar_rect();
  ui::gfx::Color fill = t.control_fill;
  if (dragging_ || is_pressed()) {
    fill = t.control_press;
  } else if (is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(bar.x, bar.y, bar.width, bar.height, fill);
}


std::string_view Splitter::paint_role() const {
  return "splitter";
}
}  // namespace views
}  // namespace ui

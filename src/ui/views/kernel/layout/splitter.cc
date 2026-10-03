// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout/splitter.h"

#include <algorithm>
#include <cmath>

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

void Splitter::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  if (old_scale <= 0.f || new_scale <= 0.f || old_scale == new_scale) {
    return;
  }
  // Preferred sizes scale with DPI; a one-shot seed taken at scale 1.0 would
  // otherwise leave Catalog at 240/288 CSS-px while Map|Data|3D tabs paint at
  // 1.5x — OS clicks aimed at DIP×scale miss the packed 3D cell.
  if (!user_adjusted_) {
    split_seeded_ = false;
    mark_needs_layout();
    return;
  }
  const float ratio = new_scale / old_scale;
  primary_extent_ =
      static_cast<int>(std::lround(primary_extent_ * ratio));
  fixed_secondary_px_ =
      static_cast<int>(std::lround(fixed_secondary_px_ * ratio));
  saved_primary_ = static_cast<int>(std::lround(saved_primary_ * ratio));
  last_main_ = static_cast<int>(std::lround(last_main_ * ratio));
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
  if (child_count() < 2) {
    return;
  }
  // layout() can run before the host has a real size; do not lock a 0 split.
  if (main_extent() <= kBarPx) {
    return;
  }
  View* a = child_at(0);
  View* b = child_at(1);
  // Seed from preferred_size() hints only. Markup hosts wrap Catalog / Map
  // tabs in FillLayout+Yoga whose get_preferred_size() is often 0 (width:100%
  // of undefined) — using that for the both-zero test seeded primary=full and
  // collapsed Map|Data|3D (hollow grey slab in ui.shell BMPs).
  const Size sa_hint = a->preferred_size();
  const Size sb_hint = b->preferred_size();
  const int pa_hint = is_horizontal() ? axis_traits<Axis::kHorizontal>::main(sa_hint)
                                      : axis_traits<Axis::kVertical>::main(sa_hint);
  const int pb_hint = is_horizontal() ? axis_traits<Axis::kHorizontal>::main(sb_hint)
                                      : axis_traits<Axis::kVertical>::main(sb_hint);

  // One-shot seed can lock both-flex (primary=full) before Catalog preferred
  // width is visible, leaving a hollow grey slab beside a squeezed map. When
  // the user has not dragged, refresh from current hints.
  // Also recover when the first layout clamped PrimaryFixed to kMinPanePx
  // while the host was still tiny — otherwise catalog stays 40px forever
  // (child-outside-parent + clipped Layers/Sources labels).
  if (split_seeded_ && !user_adjusted_ && !collapsed_) {
    if (resize_policy_ == ResizePolicy::kSecondaryFixed &&
        fixed_secondary_px_ <= 0 && pb_hint > 0) {
      // Diagnostic Tools preferred arrived after a zero-secondary seed (collapsed
      // strip). Must reseed — the old pb_hint<=0 guard never fired once the
      // panel opened, leaving work at kMinPanePx while Console stayed crushed
      // or the inverse after a bad first layout.
      split_seeded_ = false;
    } else if (resize_policy_ == ResizePolicy::kSecondaryFixed &&
               fixed_secondary_px_ <= 0 && pa_hint > 0 && pb_hint <= 0) {
      split_seeded_ = false;
    } else if (resize_policy_ == ResizePolicy::kPrimaryFixed && pa_hint > 0) {
      const int inner = std::max(0, main_extent() - kBarPx);
      const int want = std::min(pa_hint, std::max(0, inner - kMinPanePx));
      if (want > 0 && primary_extent_ != want) {
        primary_extent_ = want;
        last_main_ = main_extent();
        return;
      }
      return;
    } else {
      return;
    }
  } else if (split_seeded_) {
    return;
  }

  const int inner = std::max(0, main_extent() - kBarPx);
  fixed_secondary_px_ = 0;
  if (pa_hint <= 0 && pb_hint <= 0) {
    // Both flex / hidden: primary keeps the work area; secondary stays at 0
    // until preferred size or a user drag grows it (Diagnostic Tools starts
    // at preferred 0 — must not seed a 50/50 split that starves the map).
    primary_extent_ = inner;
    fixed_secondary_px_ = 0;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  } else if (pa_hint <= 0) {
    // BrowserView pattern: flexible map/work pane + fixed ambox/inspector /
    // Diagnostic Tools. Cap secondary so work keeps ≥1/3 of the host — a raw
    // pb_hint can exceed a create-time tiny inner and clamp primary to
    // kMinPanePx (40px), leaving Console tall but Map/FeatureInfo crushed
    // (ui.shell map hwnd client ~1664x105).
    const int min_primary = std::max(kMinPanePx, inner / 3);
    const int max_secondary = std::max(kMinPanePx, inner - min_primary);
    fixed_secondary_px_ = std::min(pb_hint, max_secondary);
    primary_extent_ = inner - fixed_secondary_px_;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  } else if (pb_hint <= 0) {
    // Catalog (fixed preferred) + map tabs (flex).
    primary_extent_ = pa_hint;
    resize_policy_ = ResizePolicy::kPrimaryFixed;
  } else {
    primary_extent_ = inner * pa_hint / (pa_hint + pb_hint);
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
  // Idle bar uses hover tone so map|inspector seams stay visible on dark panels.
  ui::gfx::Color fill = t.control_hover;
  if (dragging_ || is_pressed()) {
    fill = t.accent;
  } else if (is_hovered()) {
    fill = ui::gfx::color_rgb(0, 100, 170);
  }
  canvas->fill_rect(bar.x, bar.y, bar.width, bar.height, fill);
  // Center grip: 3px accent rail (was 1px hairline — nearly invisible).
  if (is_horizontal()) {
    const int grip = std::min(3, std::max(1, bar.width));
    canvas->fill_rect(bar.x + (bar.width - grip) / 2, bar.y + 2, grip,
                      std::max(0, bar.height - 4), t.text_muted);
  } else {
    const int grip = std::min(3, std::max(1, bar.height));
    canvas->fill_rect(bar.x + 2, bar.y + (bar.height - grip) / 2,
                      std::max(0, bar.width - 4), grip, t.text_muted);
  }
}


std::string_view Splitter::paint_role() const {
  return "splitter";
}
}  // namespace views
}  // namespace ui

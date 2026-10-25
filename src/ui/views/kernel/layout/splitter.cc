// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout/splitter.h"

#include <algorithm>
#include <cmath>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/splitter_grip.h"
#include "ui/views/kernel/layout/view_traits.h"
#include "ui/views/kernel/shell/theme.h"

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

Splitter::~Splitter() = default;

bool Splitter::is_collapsed() const {
  return collapsed_;
}

bool Splitter::is_horizontal_split() const {
  return orientation_ == Orientation::kHorizontal;
}

bool Splitter::is_grip_dragging() const {
  return dragging_;
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
    // Collapsed before the first real layout — re-seed from preferred sizes.
    split_seeded_ = false;
  }
  collapsed_ = collapsed;
  layout();
  schedule_paint();
}

void Splitter::reseed(bool force) {
  if (user_adjusted_ && !force) {
    // Keep the user seam; only re-apply geometry (DPI / host size).
    layout();
    schedule_paint();
    return;
  }
  user_adjusted_ = false;
  split_seeded_ = false;
  layout();
  schedule_paint();
}

void Splitter::enforce_primary_floor(float min_primary_frac) {
  if (pane_count() < 2 || collapsed_) {
    return;
  }
  if (min_primary_frac < 0.5f) {
    min_primary_frac = 0.5f;
  }
  if (min_primary_frac > 0.9f) {
    min_primary_frac = 0.9f;
  }
  user_adjusted_ = false;
  const int inner = std::max(0, main_extent() - kBarPx);
  if (inner <= kBarPx) {
    return;
  }
  const int min_primary =
      std::max(kMinPanePx, static_cast<int>(inner * min_primary_frac));
  const int max_secondary = std::max(0, inner - min_primary);
  if (fixed_secondary_px_ <= 0) {
    // Collapsed dock: keep primary full (Diagnostic hidden for FlyCube Init).
    primary_extent_ = inner;
  } else if (fixed_secondary_px_ > max_secondary ||
             primary_extent_ < min_primary) {
    fixed_secondary_px_ = std::min(fixed_secondary_px_, max_secondary);
    primary_extent_ = inner - fixed_secondary_px_;
  }
  resize_policy_ = ResizePolicy::kSecondaryFixed;
  split_seeded_ = true;
  last_main_ = main_extent();
  clamp_primary();
  apply_child_bounds();
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (!child) {
      continue;
    }
    if (child->is_visible()) {
      child->layout();
    }
    child->sync_native_bounds();
  }
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

View* Splitter::pane_at(size_t index) const {
  size_t n = 0;
  for (size_t i = 0; i < child_count(); ++i) {
    View* c = child_at(i);
    if (!c || c == grip_) {
      continue;
    }
    if (n == index) {
      return c;
    }
    ++n;
  }
  return nullptr;
}

size_t Splitter::pane_count() const {
  size_t n = 0;
  for (size_t i = 0; i < child_count(); ++i) {
    View* c = child_at(i);
    if (c && c != grip_) {
      ++n;
    }
  }
  return n;
}

void Splitter::ensure_grip() {
  if (pane_count() < 2) {
    if (grip_) {
      grip_->set_visible(false);
    }
    return;
  }
  if (!grip_) {
    auto g = std::make_unique<SplitterGrip>(this);
    grip_ = g.get();
    add_child(std::move(g));
  } else if (child_count() == 0 || child_at(child_count() - 1) != grip_) {
    // Keep grip last so reverse hit-test wins over pane / HWND edges.
    auto owned = remove_child(grip_);
    grip_ = static_cast<SplitterGrip*>(owned.get());
    add_child(std::move(owned));
  }
  grip_->set_visible(!collapsed_);
}

void Splitter::seed_split_if_needed() {
  if (pane_count() < 2) {
    return;
  }
  // layout() can run before the host has a real size; do not lock a 0 split.
  if (main_extent() <= kBarPx) {
    return;
  }
  View* a = pane_at(0);
  View* b = pane_at(1);
  if (!a || !b) {
    return;
  }
  // Prefer the larger of stored preferred_size() and layout-aware
  // get_preferred_size(). FillLayout hosts often keep a pinned stored hint
  // (diagnostic_host=140) while get_preferred_size() follows an inner child
  // that was left at an older markup preferred (120) — using only stored was
  // fine, but using only layout under-sized the Console strip; max() keeps
  // Catalog/Map seeds honest when either side is still 0.
  const Size sa_stored = a->preferred_size();
  const Size sb_stored = b->preferred_size();
  const Size sa_layout = a->get_preferred_size();
  const Size sb_layout = b->get_preferred_size();
  const auto main_of = [this](const Size& s) {
    return is_horizontal() ? axis_traits<Axis::kHorizontal>::main(s)
                           : axis_traits<Axis::kVertical>::main(s);
  };
  const int pa_stored = main_of(sa_stored);
  const int pb_stored = main_of(sb_stored);
  const int pa_hint = std::max(pa_stored, main_of(sa_layout));
  const int pb_hint = std::max(pb_stored, main_of(sb_layout));

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
    } else if (resize_policy_ == ResizePolicy::kSecondaryFixed &&
               pb_stored > 0 && fixed_secondary_px_ > 0 &&
               fixed_secondary_px_ + 8 < pb_stored) {
      // Refresh only when STORED preferred grew (DPI / explicit pin). Do not
      // chase FillLayout→TabStrip layout measure — long inspector titles made
      // pb_hint≫dock width and stole the map column (ui.shell Feature slab).
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
  // Flex primary (stored preferred 0) + docked secondary: map_column Yoga
  // get_preferred_size() often reports nested catalog width (>0), which used
  // to fall into Proportional and crush the map beside a wide inspector
  // TabStrip (ui.shell #2/#4 — Feature dock filled the work area).
  // Do not require pb_stored>0: FillLayout diagnostic_host often keeps stored
  // 0 while get_preferred_size() follows a tall Console child — the old guard
  // fell into Proportional and left Map/3D at ~99px (ui.interact worldpass).
  const bool flex_primary_fixed_secondary =
      pa_stored <= 0 && pb_hint > 0;
  auto seed_secondary_fixed = [&](int secondary_want) {
    const int min_primary = std::max(kMinPanePx, (inner * 2) / 3);
    const int max_secondary = std::max(kMinPanePx, inner - min_primary);
    fixed_secondary_px_ = std::min(std::max(0, secondary_want), max_secondary);
    primary_extent_ = inner - fixed_secondary_px_;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  };
  if (pa_hint <= 0 && pb_hint <= 0) {
    // Both flex / hidden: primary keeps the work area; secondary stays at 0
    // until preferred size or a user drag grows it (Diagnostic Tools starts
    // at preferred 0 — must not seed a 50/50 split that starves the map).
    primary_extent_ = inner;
    fixed_secondary_px_ = 0;
    resize_policy_ = ResizePolicy::kSecondaryFixed;
  } else if (pa_hint <= 0 || flex_primary_fixed_secondary) {
    // BrowserView pattern: flexible map/work pane + fixed ambox/inspector /
    // Diagnostic Tools. Cap secondary so work keeps ≥2/3 of the host — a raw
    // pb_hint (360 console) used to leave Map/FeatureInfo crushed in the
    // upper third (ui.shell vertical collapse / black dead zone).
    // Prefer stored secondary (inspector 320) over inflated TabStrip layout.
    seed_secondary_fixed(pb_stored > 0 ? pb_stored : pb_hint);
  } else if (pb_hint <= 0) {
    // Catalog (fixed preferred) + map tabs (flex).
    primary_extent_ = pa_hint;
    resize_policy_ = ResizePolicy::kPrimaryFixed;
  } else {
    // Both sides report hints. If proportional would starve primary (<2/3),
    // treat secondary as a dock (Diagnostic / Inspector) instead.
    const int proportional = inner * pa_hint / (pa_hint + pb_hint);
    const int min_primary = std::max(kMinPanePx, (inner * 2) / 3);
    if (proportional < min_primary) {
      seed_secondary_fixed(pb_stored > 0 ? pb_stored : pb_hint);
    } else {
      primary_extent_ = proportional;
      resize_policy_ = ResizePolicy::kProportional;
    }
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
  // SecondaryFixed / PrimaryFixed docks keep policy caps even after a grip
  // drag. map_client / OS-resize mis-hits used to set user_adjusted_ and then
  // proportional-scale a ~99px Map/3D seam forever (ui.interact FlyCube stub).
  if (resize_policy_ == ResizePolicy::kSecondaryFixed) {
    // Match seed_split_if_needed: keep primary ≥2/3 of inner. A raw
    // fixed_secondary_px_ (Diagnostic ~500) after window(resize) to 960x640
    // used to leave Map/3D at ~40x93 and FlyCube Init a stub swapchain
    // (ui.interact Phase A2→B).
    if (fixed_secondary_px_ <= 0) {
      primary_extent_ = inner;
    } else {
      const int min_primary = std::max(kMinPanePx, (inner * 2) / 3);
      const int max_secondary = std::max(0, inner - min_primary);
      primary_extent_ = inner - std::min(fixed_secondary_px_, max_secondary);
    }
  } else if (user_adjusted_ || resize_policy_ == ResizePolicy::kProportional) {
    if (last_inner > 0) {
      primary_extent_ = inner * primary_extent_ / last_inner;
    }
  } else if (resize_policy_ == ResizePolicy::kPrimaryFixed) {
    // Catalog|Map: keep catalog preferred, but never leave Map at kMinPanePx
    // after a large-window seed (catalog_w≈750) + window(resize) to 960 —
    // secondary used to absorb the entire shrink (FlyCube client 40x93).
    const int min_secondary = std::max(kMinPanePx, inner / 3);
    const int max_primary = std::max(0, inner - min_secondary);
    if (primary_extent_ > max_primary) {
      primary_extent_ = max_primary;
    }
  }
  // kPrimaryFixed growth: primary_extent_ stays; secondary absorbs (above).
  last_main_ = main;
}

void Splitter::clamp_primary() {
  const int main = main_extent();
  if (collapsed_) {
    primary_extent_ = std::max(0, main - kBarPx);
    return;
  }
  // Live grip drag: min panes only so the seam follows the pointer. After
  // release (user_adjusted_), fall through to policy clamps — otherwise a
  // mis-hit drag can lock Map/3D at kMinPanePx under Diagnostic/Inspector.
  if (dragging_) {
    const int inner = std::max(0, main - kBarPx);
    if (main < kMinPanePx * 2 + kBarPx) {
      primary_extent_ = std::max(0, std::min(primary_extent_, main - kBarPx));
      return;
    }
    const int max_primary = inner - kMinPanePx;
    primary_extent_ =
        std::max(kMinPanePx, std::min(primary_extent_, max_primary));
    return;
  }
  // Preference-0 secondary (DiagnosticToolsPanel collapsed): keep secondary at
  // 0px. Forcing kMinPanePx here left a ~40px horizon strip that painted over
  // the status bar (Diagnostic Tools / tab bleed).
  if (resize_policy_ == ResizePolicy::kSecondaryFixed &&
      fixed_secondary_px_ <= 0) {
    primary_extent_ = std::max(0, main - kBarPx);
    return;
  }
  const int inner = std::max(0, main - kBarPx);
  if (resize_policy_ == ResizePolicy::kSecondaryFixed &&
      fixed_secondary_px_ > 0) {
    const int min_primary = std::max(kMinPanePx, (inner * 2) / 3);
    const int max_secondary = std::max(0, inner - min_primary);
    const int secondary = std::min(fixed_secondary_px_, max_secondary);
    primary_extent_ = std::max(min_primary, inner - secondary);
    return;
  }
  if (resize_policy_ == ResizePolicy::kPrimaryFixed) {
    const int min_secondary = std::max(kMinPanePx, inner / 3);
    const int max_primary = std::max(0, inner - min_secondary);
    primary_extent_ =
        std::max(kMinPanePx, std::min(primary_extent_, max_primary));
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
  View* a = pane_at(0);
  View* sec = pane_at(1);
  const int main = is_horizontal() ? b.width : b.height;
  int primary = primary_extent_;
  if (main > kBarPx) {
    primary = std::max(0, std::min(primary, main - kBarPx));
  } else {
    primary = 0;
  }
  if (is_horizontal()) {
    place_panes<Axis::kHorizontal>(a, sec, b, primary, kBarPx);
  } else {
    place_panes<Axis::kVertical>(a, sec, b, primary, kBarPx);
  }
  if (grip_) {
    grip_->set_bounds(bar_rect());
  }
}

void Splitter::layout() {
  ensure_grip();
  seed_split_if_needed();
  adjust_for_host_resize();
  clamp_primary();
  apply_child_bounds();
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (!child) {
      continue;
    }
    if (child->is_visible()) {
      child->layout();
    }
    child->sync_native_bounds();
  }
}

bool Splitter::on_mouse_event(const MouseEvent& event) {
  return View::on_mouse_event(event);
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
  if (resize_policy_ == ResizePolicy::kSecondaryFixed) {
    const int inner = std::max(0, main_extent() - kBarPx);
    fixed_secondary_px_ = std::max(0, inner - primary_extent_);
  }
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

void Splitter::end_drag() {
  dragging_ = false;
  if (resize_policy_ == ResizePolicy::kSecondaryFixed) {
    const int inner = std::max(0, main_extent() - kBarPx);
    fixed_secondary_px_ = std::max(0, inner - primary_extent_);
  }
}

bool Splitter::on_grip_mouse(const MouseEvent& event) {
  if (!is_enabled() || !is_visible()) {
    return false;
  }
  const int pos = is_horizontal() ? event.x : event.y;
  if (dragging_) {
    if (event.type == MouseEvent::Type::kMove ||
        event.type == MouseEvent::Type::kUp) {
      update_drag(pos);
      if (event.type == MouseEvent::Type::kUp) {
        end_drag();
      }
      return true;
    }
  }
  SetCursor(LoadCursor(nullptr, is_horizontal() ? IDC_SIZEWE : IDC_SIZENS));
  if (event.type == MouseEvent::Type::kDown && event.button == 1) {
    begin_drag(pos);
    return true;
  }
  return event.type == MouseEvent::Type::kMove ||
         event.type == MouseEvent::Type::kDown;
}

void Splitter::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

std::string_view Splitter::paint_role() const {
  return "splitter";
}

std::unique_ptr<Splitter> make_splitter(Splitter::Orientation orientation) {
  return std::make_unique<Splitter>(orientation);
}

Splitter* splitter_new(Splitter::Orientation orientation) {
  return new Splitter(orientation);
}

}  // namespace views
}  // namespace ui

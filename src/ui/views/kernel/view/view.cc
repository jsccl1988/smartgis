// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/view/view.h"

#include <cmath>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/paint/painter.h"
#include "ui/views/kernel/paint/painter_registry.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

bool clip_misses(ui::gfx::Canvas* canvas, const Rect& bounds) {
  if (!canvas || !canvas->hdc() || bounds.width <= 0 || bounds.height <= 0) {
    return false;
  }
  RECT box = {};
  const int kind = GetClipBox(canvas->hdc(), &box);
  if (kind == NULLREGION) {
    return true;
  }
  if (kind == ERROR) {
    return false;
  }
  const Rect clip{box.left, box.top, box.right - box.left, box.bottom - box.top};
  return !bounds.intersects(clip);
}

}  // namespace

View::View() = default;

View::~View() {
  // Drop children before the rest of View members. Subclasses that wire child
  // callbacks capturing `this` should still clear those callbacks in their own
  // destructor before their std::function members run.
  children_.clear();
  if (widget_) {
    widget_->clear_view_refs(this);
  }
  if (native_hwnd_ && IsWindow(native_hwnd_)) {
    DestroyWindow(native_hwnd_);
  }
}

void View::add_child(std::unique_ptr<View> child) {
  if (!child) {
    return;
  }
  child->parent_ = this;
  child->set_widget(widget_);
  children_.push_back(std::move(child));
  mark_needs_layout();
}

std::unique_ptr<View> View::remove_child(View* child) {
  if (!child) {
    return nullptr;
  }
  for (auto it = children_.begin(); it != children_.end(); ++it) {
    if (it->get() != child) {
      continue;
    }
    std::unique_ptr<View> out = std::move(*it);
    children_.erase(it);
    out->parent_ = nullptr;
    out->set_widget(nullptr);
    mark_needs_layout();
    return out;
  }
  return nullptr;
}

void View::remove_all_children() {
  children_.clear();
}

View* View::child_at(size_t i) const {
  return i < children_.size() ? children_[i].get() : nullptr;
}

void View::set_bounds(const Rect& bounds) {
  const bool moved = bounds.x != bounds_.x || bounds.y != bounds_.y ||
                     bounds.width != bounds_.width ||
                     bounds.height != bounds_.height;
  if (!moved) {
    sync_native_bounds();
    return;
  }
  const Rect old = bounds_;
  bounds_ = bounds;
  commands_dirty_ = true;
  // Move (not only resize) must reflow children: TabStrip pages / ScrollView
  // content are placed from this view's origin. Skipping layout on a pure move
  // left MapViewport HWND at the old Y covering the tab headers — clicks never
  // reached Map|Data|3D (双击壳里切不了 3D).
  if (moved) {
    mark_needs_layout();
    // Eager layout when a direct child owns an HWND so native bounds track
    // before the next WM_PAINT (mouse can hit the stale HWND first).
    if (!in_layout_) {
      bool hwnd_child = native_hwnd_ != nullptr;
      if (!hwnd_child) {
        for (const auto& child : children_) {
          if (child && child->native_view()) {
            hwnd_child = true;
            break;
          }
        }
      }
      if (hwnd_child) {
        layout();
      }
    }
  }
  sync_native_bounds();
  // Layout growth/shrink must invalidate old+new — otherwise newly exposed
  // chrome stays stale until a hover schedule_paint_rect hits it.
  if (widget_) {
    if (old.width > 0 && old.height > 0) {
      widget_->schedule_paint_rect(old);
    }
    if (bounds_.width > 0 && bounds_.height > 0) {
      widget_->schedule_paint_rect(bounds_);
    }
  }
}

void View::set_preferred_size(const Size& size) {
  if (preferred_size_.width == size.width &&
      preferred_size_.height == size.height) {
    return;
  }
  preferred_size_ = size;
  if (parent_) {
    parent_->mark_needs_layout();
  }
}

void View::set_layout_manager(std::unique_ptr<LayoutManager> layout) {
  layout_ = std::move(layout);
  mark_needs_layout();
}

Size View::get_preferred_size() const {
  if (layout_) {
    return layout_->get_preferred_size(this);
  }
  return preferred_size_;
}

void View::set_widget(Widget* widget) {
  widget_ = widget;
  for (auto& child : children_) {
    child->set_widget(widget);
  }
  // Controls measure at scale 1.f in their ctor (no widget yet). When the
  // tree attaches to a DPI-scaled Widget, rebuild DIP metrics once so chrome
  // text is not stuck tiny until a later hover/scroll paint.
  if (widget_ && widget_->device_scale_factor() > 0.f) {
    const float scale = widget_->device_scale_factor();
    if (std::fabs(scale - 1.f) >= 0.0001f) {
      on_device_scale_factor_changed(1.f, scale);
    }
  }
}

void View::set_visible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  // Descendants with HWNDs (map panes under a hidden tab) must re-sync:
  // is_visible() walks ancestors, so only updating this node leaks natives.
  sync_native_tree();
  commands_dirty_ = true;
  mark_needs_layout();
  schedule_paint();
}

bool View::is_visible() const {
  return visible_ && (!parent_ || parent_->is_visible());
}

void View::set_enabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_ && is_focused()) {
    if (widget_) {
      widget_->set_focused_view(nullptr);
    }
  }
  commands_dirty_ = true;
  schedule_paint();
}

bool View::is_enabled() const {
  return enabled_ && (!parent_ || parent_->is_enabled());
}

void View::set_focusable(bool focusable) {
  focusable_ = focusable;
  if (!focusable_ && is_focused() && widget_) {
    widget_->set_focused_view(nullptr);
  }
}

bool View::is_focused() const {
  return widget_ && widget_->focused_view() == this;
}

bool View::request_focus() {
  if (!is_focusable() || !is_visible() || !is_enabled() || !widget_) {
    return false;
  }
  widget_->set_focused_view(this);
  return true;
}

void View::set_hovered(bool hovered) {
  if (hovered_ == hovered) {
    return;
  }
  hovered_ = hovered;
  commands_dirty_ = true;
  schedule_paint();
}

void View::set_pressed(bool pressed) {
  if (pressed_ == pressed) {
    return;
  }
  pressed_ = pressed;
  commands_dirty_ = true;
  schedule_paint();
}

void View::invalidate() {
  schedule_paint();
}

void View::schedule_paint() {
  if (!widget_) {
    return;
  }
  // Dirty only this view's bounds. Full-client InvalidateRect on every hover
  // made Skia shell flash the entire window while the mouse moved.
  if (bounds_.width > 0 && bounds_.height > 0) {
    widget_->schedule_paint_rect(bounds_);
  } else {
    widget_->schedule_paint();
  }
}

void View::invalidate_commands() {
  commands_dirty_ = true;
}

void View::mark_needs_layout() {
  if (in_layout_) {
    return;
  }
  if (needs_layout_) {
    return;
  }
  needs_layout_ = true;
  if (parent_) {
    parent_->mark_needs_layout();
  }
}

void View::set_exposed_rect(const Rect& rect) {
  if (exposed_.x == rect.x && exposed_.y == rect.y &&
      exposed_.width == rect.width && exposed_.height == rect.height) {
    return;
  }
  exposed_ = rect;
  commands_dirty_ = true;
}

void View::layout() {
  ui::gfx::note_layout();
  LayoutScope scope(this);
  if (layout_) {
    layout_->layout(this);
  }
  for (auto& child : children_) {
    if (!child->visible_) {
      continue;
    }
    child->layout();
    child->sync_native_bounds();
  }
  sync_native_bounds();
}

void View::ensure_commands_recorded() {
  if (!commands_dirty_ && commands_ready_) {
    return;
  }
  commands_.clear();
  // Null HDC: Canvas draw ops only append to the thread_local recorder.
  ui::gfx::Canvas recorder(nullptr, bounds_.width, bounds_.height);
  ui::gfx::display_list_begin(&commands_);
  if (paint_delegate_) {
    paint_delegate_->paint_before(this, &recorder);
  }
  const std::string_view role = paint_role();
  if (Painter* painter = PainterRegistry::get().find(role)) {
    painter->paint(this, &recorder);
  } else {
    paint_self(&recorder);
  }
  if (paint_delegate_) {
    paint_delegate_->paint_after(this, &recorder);
  }
  ui::gfx::display_list_end();
  commands_ready_ = true;
  commands_dirty_ = false;
}

void View::append_commands_to(ui::gfx::DisplayList* out,
                              const Rect* dirty_or_null) {
  if (!out || !visible_) {
    return;
  }
  const bool cull =
      dirty_or_null && !dirty_or_null->is_empty();
  if (cull && !bounds_.intersects(*dirty_or_null)) {
    return;
  }
  ensure_commands_recorded();
  // Match View::paint: clip each subtree so compositor commits cannot bleed
  // sibling chrome (UiDesigner canvas / shell panels).
  const bool clip = bounds_.width > 0 && bounds_.height > 0;
  if (clip) {
    out->save();
    out->clip_rect(bounds_.x, bounds_.y, bounds_.width, bounds_.height);
  }
  out->append_from(commands_);
  for (auto& child : children_) {
    if (!child->visible_) {
      continue;
    }
    if (child->native_view()) {
      // Parent fills often paint opaque panel_bg over the map HWND rect.
      // Overlay punch only knows Theme hole colors — stamp map_placeholder so
      // src-over does not flash a correct GPU frame then cover it (错位).
      const Rect& b = child->bounds();
      if (b.width > 0 && b.height > 0 &&
          (!cull || b.intersects(*dirty_or_null))) {
        out->fill_rect(b.x, b.y, b.width, b.height,
                       Theme::current().map_placeholder);
      }
      continue;
    }
    child->append_commands_to(out, dirty_or_null);
  }
  if (clip) {
    out->restore();
  }
}

void View::paint_commands(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  ensure_commands_recorded();
  RECT box = {};
  const int kind = canvas->hdc() ? GetClipBox(canvas->hdc(), &box) : ERROR;
  if (kind == NULLREGION) {
    return;
  }
  if (kind == SIMPLEREGION || kind == COMPLEXREGION) {
    commands_.replay_clipped(canvas, box.left, box.top, box.right, box.bottom);
    return;
  }
  commands_.replay(canvas);
}

void View::paint(ui::gfx::Canvas* canvas) {
  if (!canvas || !visible_) {
    return;
  }
  if (bounds_.width > 0 && bounds_.height > 0 && clip_misses(canvas, bounds_)) {
    return;
  }
  // Clip to this view's bounds so shell text / fills cannot bleed into
  // siblings (industry visual-layout invariant: no paint overflow).
  canvas->save();
  if (bounds_.width > 0 && bounds_.height > 0) {
    canvas->clip_rect(bounds_.x, bounds_.y, bounds_.width, bounds_.height);
  }
  paint_commands(canvas);
  for (auto& child : children_) {
    if (!child->visible_ || child->native_view()) {
      continue;
    }
    child->paint(canvas);
  }
  canvas->restore();
}

bool View::on_mouse_event(const MouseEvent& event) {
  if (!visible_ || !is_enabled()) {
    return false;
  }
  View* hit = get_view_at(event.x, event.y);
  if (hit && hit != this) {
    return hit->on_mouse_event(event);
  }
  return false;
}

bool View::on_key_event(const KeyEvent& event) {
  if (!visible_ || !is_enabled()) {
    return false;
  }
  for (auto& child : children_) {
    if (child->on_key_event(event)) {
      return true;
    }
  }
  return false;
}

bool View::on_char_event(const CharEvent& event) {
  if (!visible_ || !is_enabled()) {
    return false;
  }
  for (auto& child : children_) {
    if (child->on_char_event(event)) {
      return true;
    }
  }
  return false;
}

void View::on_focus() {}

void View::on_blur() {}

void View::on_device_scale_factor_changed(float old_scale, float new_scale) {
  if (old_scale <= 0.f || new_scale <= 0.f || old_scale == new_scale) {
    return;
  }
  const float ratio = new_scale / old_scale;
  preferred_size_.width =
      static_cast<int>(std::lround(preferred_size_.width * ratio));
  preferred_size_.height =
      static_cast<int>(std::lround(preferred_size_.height * ratio));
}

void View::propagate_device_scale_factor_changed(float old_scale,
                                               float new_scale) {
  on_device_scale_factor_changed(old_scale, new_scale);
  for (auto& child : children_) {
    child->propagate_device_scale_factor_changed(old_scale, new_scale);
  }
}

View* View::get_view_at(int x, int y) {
  if (!visible_ || !bounds_.contains(x, y)) {
    return nullptr;
  }
  for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
    if (View* hit = (*it)->get_view_at(x, y)) {
      return hit;
    }
  }
  return this;
}

void View::realize_native() {
  if (native_hwnd_ || !widget_ || !widget_->hwnd()) {
    return;
  }
  native_hwnd_ = create_native_view(widget_->hwnd());
  sync_native_bounds();
}

void View::realize_native_tree() {
  realize_native();
  for (auto& child : children_) {
    child->realize_native_tree();
  }
}

void View::sync_native_bounds() {
  if (!native_hwnd_ || !IsWindow(native_hwnd_)) {
    return;
  }
  RECT wr = {};
  GetWindowRect(native_hwnd_, &wr);
  POINT tl = {wr.left, wr.top};
  HWND parent = GetParent(native_hwnd_);
  if (parent) {
    ScreenToClient(parent, &tl);
  }
  const int cur_w = wr.right - wr.left;
  const int cur_h = wr.bottom - wr.top;
  const bool shown = IsWindowVisible(native_hwnd_) != FALSE;
  const bool want_show = is_visible();
  // Skip no-op SetWindowPos: repeated SWP_SHOWWINDOW thrash causes child
  // HWND flicker against the Skia shell paint path.
  if (tl.x == bounds_.x && tl.y == bounds_.y && cur_w == bounds_.width &&
      cur_h == bounds_.height && shown == want_show) {
    return;
  }
  // NOCOPYBITS: default SetWindowPos copies the old DC into the new rect,
  // which reads as a shifted region until the mouse forces a real paint.
  UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
  if (want_show != shown) {
    flags |= want_show ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
  }
  SetWindowPos(native_hwnd_, nullptr, bounds_.x, bounds_.y, bounds_.width,
               bounds_.height, flags);
  InvalidateRect(native_hwnd_, nullptr, FALSE);
}

void View::sync_native_tree() {
  sync_native_bounds();
  for (auto& child : children_) {
    child->sync_native_tree();
  }
}

HWND View::create_native_view(HWND) {
  return nullptr;
}

void View::paint_self(ui::gfx::Canvas*) {}

std::string_view View::paint_role() const {
  return {};
}

void View::set_paint_delegate(PaintDelegate* delegate) {
  paint_delegate_ = delegate;
  invalidate_commands();
}

void View::paint_contents_for_painter(ui::gfx::Canvas* canvas) {
  paint_self(canvas);
}

}  // namespace views
}  // namespace ui

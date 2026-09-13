// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/view.h"

#include "render/skia/canvas.h"
#include "ui/views/layout.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {

View::View() = default;

View::~View() {
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
}

void View::remove_all_children() {
  children_.clear();
}

View* View::child_at(size_t i) const {
  return i < children_.size() ? children_[i].get() : nullptr;
}

void View::set_bounds(const Rect& bounds) {
  bounds_ = bounds;
  sync_native_bounds();
}

void View::set_layout_manager(std::unique_ptr<LayoutManager> layout) {
  layout_ = std::move(layout);
}

void View::set_widget(Widget* widget) {
  widget_ = widget;
  for (auto& child : children_) {
    child->set_widget(widget);
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
  schedule_paint();
}

void View::set_pressed(bool pressed) {
  if (pressed_ == pressed) {
    return;
  }
  pressed_ = pressed;
  schedule_paint();
}

void View::invalidate() {
  schedule_paint();
}

void View::schedule_paint() {
  if (widget_) {
    widget_->schedule_paint();
  }
}

void View::layout() {
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

void View::paint(render::skia::Canvas* canvas) {
  if (!canvas || !visible_) {
    return;
  }
  paint_self(canvas);
  for (auto& child : children_) {
    if (!child->visible_ || child->native_view()) {
      continue;
    }
    child->paint(canvas);
  }
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
  UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
  flags |= is_visible() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
  SetWindowPos(native_hwnd_, nullptr, bounds_.x, bounds_.y, bounds_.width,
               bounds_.height, flags);
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

void View::paint_self(render::skia::Canvas*) {}

}  // namespace views
}  // namespace ui

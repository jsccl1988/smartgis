// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/collection/scroll_view.h"

#include <algorithm>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kWheelStep = 40;
constexpr int kTrackW = 12;

Rect union_rect(const Rect& a, const Rect& b) {
  if (a.width <= 0 || a.height <= 0) {
    return b;
  }
  if (b.width <= 0 || b.height <= 0) {
    return a;
  }
  const int l = std::min(a.x, b.x);
  const int t = std::min(a.y, b.y);
  const int r = std::max(a.right(), b.right());
  const int bot = std::max(a.bottom(), b.bottom());
  return {l, t, r - l, bot - t};
}

bool rect_contains(const Rect& outer, const Rect& inner) {
  if (inner.width <= 0 || inner.height <= 0) {
    return true;
  }
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
}

}  // namespace

// Track and thumb for an existing ScrollView. Not a separate control library.
class ScrollView::Track : public View {
 public:
  explicit Track(ScrollView* host) : host_(host) {}

  void set_thumb(int content_h, int view_h, int scroll_y) {
    const Rect& b = bounds();
    if (view_h <= 0 || content_h <= view_h || b.height <= 0) {
      thumb_ = {};
      invalidate_commands();
      return;
    }
    const int thumb_h = std::max(12, b.height * view_h / content_h);
    const int travel = std::max(0, b.height - thumb_h);
    const int max_scroll = content_h - view_h;
    const int y = max_scroll > 0 ? travel * scroll_y / max_scroll : 0;
    const Rect next{b.x, b.y + y, b.width, thumb_h};
    if (next.x != thumb_.x || next.y != thumb_.y || next.width != thumb_.width ||
        next.height != thumb_.height) {
      thumb_ = next;
      invalidate_commands();
    }
  }

  Rect thumb_rect() const { return thumb_; }

  bool on_mouse_event(const MouseEvent& event) override {
    if (!host_ || !is_visible() || !is_enabled()) {
      return false;
    }
    if (event.type == MouseEvent::Type::kWheel) {
      return host_->on_mouse_event(event);
    }
    if (event.type == MouseEvent::Type::kDown && event.button == 1) {
      dragging_ = true;
      host_->seek_scroll_from_y(event.y);
      return true;
    }
    if (event.type == MouseEvent::Type::kMove && dragging_) {
      host_->seek_scroll_from_y(event.y);
      return true;
    }
    if (event.type == MouseEvent::Type::kUp && event.button == 1 && dragging_) {
      dragging_ = false;
      host_->seek_scroll_from_y(event.y);
      return true;
    }
    return false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_header);
    if (thumb_.width > 0 && thumb_.height > 0) {
      canvas->fill_rect(thumb_.x + 2, thumb_.y, std::max(1, thumb_.width - 4),
                        thumb_.height, t.control_hover);
    }
  }

 private:
  ScrollView* host_ = nullptr;
  bool dragging_ = false;
  Rect thumb_{};
};

ScrollView::ScrollView() {
  set_preferred_size({160, 120});
  auto track = std::make_unique<Track>(this);
  track_ = track.get();
  track_->set_visible(false);
  add_child(std::move(track));
}

View* ScrollView::content() const {
  for (size_t i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child && child != track_) {
      return child;
    }
  }
  return nullptr;
}

int ScrollView::content_height() const {
  View* child = content();
  return child ? child->preferred_size().height : 0;
}

void ScrollView::seek_scroll_from_y(int y) {
  if (!track_) {
    return;
  }
  const int content_h = content_height();
  const int view_h = bounds().height;
  if (content_h <= view_h || view_h <= 0) {
    return;
  }
  const Rect& tb = track_->bounds();
  const int thumb_h = std::max(12, tb.height * view_h / content_h);
  const int travel = std::max(1, tb.height - thumb_h);
  const int rel = std::clamp(y - tb.y - thumb_h / 2, 0, travel);
  set_scroll_offset(rel * (content_h - view_h) / travel);
}

void ScrollView::set_on_scroll(ScrollChanged fn) {
  on_scroll_ = std::move(fn);
}

void ScrollView::set_scroll_offset(int y) {
  const Rect old_thumb =
      track_ && track_->is_locally_visible()
          ? static_cast<Track*>(track_)->thumb_rect()
          : Rect{};
  const int prev = scroll_y_;
  scroll_y_ = y;
  clamp_scroll();
  apply_content_bounds();
  update_track();
  const Rect new_thumb =
      track_ && track_->is_locally_visible()
          ? static_cast<Track*>(track_)->thumb_rect()
          : Rect{};
  post_scroll_dirty(union_rect(union_rect(viewport_rect(), old_thumb), new_thumb));
  if (on_scroll_ && scroll_y_ != prev) {
    on_scroll_(scroll_y_);
  }
}

void ScrollView::clamp_scroll() {
  const int content_h = content_height();
  const int max_y = std::max(0, content_h - bounds().height);
  if (scroll_y_ < 0) {
    scroll_y_ = 0;
  } else if (scroll_y_ > max_y) {
    scroll_y_ = max_y;
  }
}

void ScrollView::apply_content_bounds() {
  View* child = content();
  if (!child) {
    return;
  }
  const Rect& b = bounds();
  const Size pref = child->get_preferred_size();
  const int w = std::max(b.width, pref.width);
  const int h = std::max(pref.height, 0);
  const bool same = applied_pref_w_ == pref.width &&
                    applied_pref_h_ == pref.height &&
                    child->bounds().width == w && child->bounds().height == h;
  child->set_bounds({b.x, b.y - scroll_y_, w, h});
  if (!same) {
    applied_pref_w_ = pref.width;
    applied_pref_h_ = pref.height;
    child->layout();
  }
}

Rect ScrollView::viewport_rect() const {
  return bounds();
}

void ScrollView::update_track() {
  if (!track_) {
    return;
  }
  const int content_h = content_height();
  const int view_h = bounds().height;
  const bool show = content_h > view_h && view_h > 0 && bounds().width > kTrackW;
  track_->set_visible(show);
  if (!show) {
    return;
  }
  track_->set_bounds(
      {bounds().right() - kTrackW, bounds().y, kTrackW, bounds().height});
  static_cast<Track*>(track_)->set_thumb(content_h, view_h, scroll_y_);
}

void ScrollView::post_scroll_dirty(const Rect& dirty) {
  if (!widget()) {
    schedule_paint();
    return;
  }
  if (!scroll_dirty_posted_) {
    scroll_dirty_ = dirty.width > 0 ? dirty : bounds();
    scroll_dirty_posted_ = true;
    widget()->schedule_paint_rect(scroll_dirty_);
    return;
  }
  const Rect next = union_rect(scroll_dirty_, dirty);
  if (rect_contains(scroll_dirty_, next)) {
    return;
  }
  scroll_dirty_ = next;
  widget()->schedule_paint_rect(scroll_dirty_);
}

void ScrollView::layout() {
  LayoutScope scope(this);
  clamp_scroll();
  apply_content_bounds();
  update_track();
}

void ScrollView::paint(ui::gfx::Canvas* canvas) {
  if (!canvas || !is_visible()) {
    return;
  }
  const Rect& b = bounds();
  if (b.width > 0 && b.height > 0 && canvas->hdc()) {
    RECT box = {};
    const int kind = GetClipBox(canvas->hdc(), &box);
    if (kind == NULLREGION) {
      return;
    }
    if (kind == SIMPLEREGION || kind == COMPLEXREGION) {
      const Rect clip{box.left, box.top, box.right - box.left,
                      box.bottom - box.top};
      if (!b.intersects(clip)) {
        return;
      }
    }
  }
  canvas->save();
  if (b.width > 0 && b.height > 0) {
    canvas->clip_rect(b.x, b.y, b.width, b.height);
  }
  paint_commands(canvas);
  View* child = content();
  if (child && child->is_visible() && !child->native_view()) {
    const Rect port = viewport_rect();
    canvas->save();
    canvas->clip_rect(port.x, port.y, port.width, port.height);
    child->set_exposed_rect(port);
    child->paint(canvas);
    canvas->restore();
  }
  if (track_ && track_->is_locally_visible()) {
    track_->paint(canvas);
  }
  canvas->restore();
  scroll_dirty_posted_ = false;
  scroll_dirty_ = {};
}

bool ScrollView::on_mouse_event(const MouseEvent& event) {
  if (!is_visible() || !is_enabled()) {
    return false;
  }
  if (event.type == MouseEvent::Type::kWheel &&
      bounds().contains(event.x, event.y)) {
    const int steps = event.wheel_delta / 120;
    const int delta = (steps == 0 && event.wheel_delta != 0)
                          ? (event.wheel_delta > 0 ? 1 : -1)
                          : steps;
    set_scroll_offset(scroll_y_ - delta * kWheelStep);
    return true;
  }
  return View::on_mouse_event(event);
}

void ScrollView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
}


std::string_view ScrollView::paint_role() const {
  return "scroll_view";
}
}  // namespace views
}  // namespace ui

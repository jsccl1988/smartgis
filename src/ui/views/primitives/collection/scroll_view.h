// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_COLLECTION_SCROLL_VIEW_H_
#define UI_VIEWS_PRIMITIVES_COLLECTION_SCROLL_VIEW_H_

#include <functional>

#include "ui/ui_export.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Clips a single child to this view's bounds and offsets it by a vertical
// scroll position. Wheel (including over content, via Widget bubble) and
// the right-edge track seek the offset.
class UI_EXPORT ScrollView : public View {
 public:
  using ScrollChanged = std::function<void(int offset)>;

  ScrollView();

  void set_scroll_offset(int y);
  int scroll_offset() const { return scroll_y_; }

  // Fired after clamp when the offset actually changes (wheel / track / API).
  void set_on_scroll(ScrollChanged fn);

  void layout() override;
  void paint(ui::gfx::Canvas* canvas) override;
  bool on_mouse_event(const MouseEvent& event) override;
  bool allows_child_overflow() const override { return true; }
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  class Track;

  View* content() const;
  int content_height() const;
  void clamp_scroll();
  void apply_content_bounds();
  void update_track();
  void seek_scroll_from_y(int y);
  Rect viewport_rect() const;
  void post_scroll_dirty(const Rect& dirty);

  View* track_ = nullptr;
  int scroll_y_ = 0;
  int applied_pref_w_ = -1;
  int applied_pref_h_ = -1;
  bool scroll_dirty_posted_ = false;
  Rect scroll_dirty_{};
  ScrollChanged on_scroll_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_COLLECTION_SCROLL_VIEW_H_

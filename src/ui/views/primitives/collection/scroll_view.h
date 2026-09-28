// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_COLLECTION_SCROLL_VIEW_H_
#define UI_VIEWS_PRIMITIVES_COLLECTION_SCROLL_VIEW_H_

#include "ui/ui_views_export.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Clips a single child to this view's bounds and offsets it by a vertical
// scroll position. Mouse wheel moves the offset.
class UI_VIEWS_EXPORT ScrollView : public View {
 public:
  ScrollView();

  void set_scroll_offset(int y);
  int scroll_offset() const { return scroll_y_; }

  void layout() override;
  void paint(ui::gfx::Canvas* canvas) override;
  bool on_mouse_event(const MouseEvent& event) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  View* content() const;
  void clamp_scroll();
  void apply_content_bounds();
  void update_track();
  Rect viewport_rect() const;
  void post_scroll_dirty(const Rect& dirty);

  View* track_ = nullptr;
  int scroll_y_ = 0;
  int applied_pref_w_ = -1;
  int applied_pref_h_ = -1;
  bool scroll_dirty_posted_ = false;
  Rect scroll_dirty_{};
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_COLLECTION_SCROLL_VIEW_H_

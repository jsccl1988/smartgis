// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_SCROLL_VIEW_H_
#define UI_VIEWS_SCROLL_VIEW_H_

#include "ui/views/view.h"

namespace ui {
namespace views {

// Clips a single child to this view's bounds and offsets it by a vertical
// scroll position. Mouse wheel moves the offset.
class ScrollView : public View {
 public:
  ScrollView();

  void set_scroll_offset(int y);
  int scroll_offset() const { return scroll_y_; }

  void layout() override;
  void paint(render::skia::Canvas* canvas) override;
  bool on_mouse_event(const MouseEvent& event) override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  View* content() const;
  void clamp_scroll();
  void apply_content_bounds();

  int scroll_y_ = 0;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_SCROLL_VIEW_H_

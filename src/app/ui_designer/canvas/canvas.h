// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_CANVAS_CANVAS_H_
#define APP_UI_DESIGNER_CANVAS_CANVAS_H_

#include <functional>
#include <memory>

#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/view/view.h"

namespace app {

constexpr int kCanvasDragThresholdPx = 4;

// Transparent top child: last in z-order so get_view_at hits it (get_view_at
// is not virtual). Captures press so Widget routes move/up here for drag.
class CanvasHitLayer : public ui::views::View {
 public:
  using MouseFn = std::function<bool(const ui::views::MouseEvent&)>;
  void set_mouse_handler(MouseFn fn) { mouse_ = std::move(fn); }
  void set_selected_bounds(ui::views::Rect r) {
    selected_bounds_ = r;
    invalidate();
  }
  void set_drop_line(ui::views::Rect r) {
    drop_line_ = r;
    invalidate();
  }
  bool on_mouse_event(const ui::views::MouseEvent& e) override {
    return mouse_ ? mouse_(e) : false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  MouseFn mouse_;
  ui::views::Rect selected_bounds_{};
  ui::views::Rect drop_line_{};
};

// Hosts markup content under a hit overlay for selection and drag horizon.
class CanvasHost : public ui::views::View {
 public:
  using MouseFn = std::function<bool(const ui::views::MouseEvent&)>;
  void set_mouse_handler(MouseFn fn);
  void set_content(std::unique_ptr<ui::views::View> content);
  ui::views::View* hit_content(int x, int y) const;
  void set_selected_bounds(ui::views::Rect r);
  void set_drop_line(ui::views::Rect r);

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  ui::views::View* content_ = nullptr;
  CanvasHitLayer* hit_ = nullptr;
  MouseFn mouse_;
  ui::views::Rect selected_bounds_{};
  ui::views::Rect drop_line_{};
};

}  // namespace app

#endif  // APP_UI_DESIGNER_CANVAS_CANVAS_H_

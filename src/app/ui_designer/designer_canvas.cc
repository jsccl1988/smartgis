// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/designer_canvas.h"

#include "ui/gfx/canvas/canvas.h"

namespace app {

void CanvasHitLayer::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const ui::views::Theme& t = ui::views::Theme::current();
  // Widget space (same as View::bounds / Label::paint_self).
  if (selected_bounds_.width > 0) {
    canvas->stroke_rect(selected_bounds_.x, selected_bounds_.y,
                        selected_bounds_.width, selected_bounds_.height,
                        t.accent, 2);
  }
  if (drop_line_.width > 0 || drop_line_.height > 0) {
    canvas->fill_rect(drop_line_.x, drop_line_.y, drop_line_.width,
                      drop_line_.height, t.accent);
  }
}

void CanvasHost::set_mouse_handler(MouseFn fn) {
  mouse_ = std::move(fn);
  if (hit_) {
    hit_->set_mouse_handler(mouse_);
  }
}

void CanvasHost::set_content(std::unique_ptr<ui::views::View> content) {
  remove_all_children();
  content_ = nullptr;
  hit_ = nullptr;
  set_layout_manager(std::make_unique<ui::views::FillLayout>());
  if (content) {
    content_ = content.get();
    add_child(std::move(content));
  }
  auto hit = std::make_unique<CanvasHitLayer>();
  hit_ = hit.get();
  if (mouse_) {
    hit_->set_mouse_handler(mouse_);
  }
  hit_->set_selected_bounds(selected_bounds_);
  hit_->set_drop_line(drop_line_);
  add_child(std::move(hit));
}

ui::views::View* CanvasHost::hit_content(int x, int y) const {
  return content_ ? content_->get_view_at(x, y) : nullptr;
}

void CanvasHost::set_selected_bounds(ui::views::Rect r) {
  selected_bounds_ = r;
  if (hit_) {
    hit_->set_selected_bounds(r);
  }
  invalidate();
}

void CanvasHost::set_drop_line(ui::views::Rect r) {
  drop_line_ = r;
  if (hit_) {
    hit_->set_drop_line(r);
  }
  invalidate();
}

void CanvasHost::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const ui::views::Theme& t = ui::views::Theme::current();
  canvas->fill_rect(bounds().x, bounds().y, bounds().width, bounds().height,
                    t.shell_bg);
}

}  // namespace app

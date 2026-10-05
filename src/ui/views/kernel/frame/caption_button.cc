// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/frame/caption_button.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

CaptionButton::CaptionButton(CaptionButtonKind kind) : kind_(kind) {
  set_focusable(false);
  rebuild_preferred();
}

void CaptionButton::set_kind(CaptionButtonKind kind) {
  kind_ = kind;
  schedule_paint();
}

void CaptionButton::set_click(std::function<void()> fn) {
  click_ = std::move(fn);
}

void CaptionButton::rebuild_preferred() {
  const float scale =
      widget() ? widget()->device_scale_factor() : 1.f;
  set_preferred_size({dip_to_px(46, scale), dip_to_px(32, scale)});
}

void CaptionButton::on_device_scale_factor_changed(float /*old_scale*/,
                                                 float /*new_scale*/) {
  rebuild_preferred();
}

void CaptionButton::activate() {
  if (click_) {
    click_();
  }
}

bool CaptionButton::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kDown && e.button == 1) {
    set_pressed(true);
    schedule_paint();
    return true;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    const bool was = is_pressed();
    set_pressed(false);
    schedule_paint();
    // Capture delivers Up to the press target even after the cursor leaves.
    // Activate only when the release is still over this control.
    if (was && bounds().contains(e.x, e.y)) {
      activate();
    }
    return true;
  }
  return false;
}

void CaptionButton::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  ui::gfx::Color fill = t.caption_bg;
  if (is_hovered() || is_pressed()) {
    fill = (kind_ == CaptionButtonKind::kClose) ? t.caption_close_hover
                                                : t.caption_button_hover;
  }
  canvas->fill_rect(b.x, b.y, b.width, b.height, fill);

  const ui::gfx::Color fg =
      (kind_ == CaptionButtonKind::kClose && (is_hovered() || is_pressed()))
          ? ui::gfx::color_rgb(255, 255, 255)
          : t.text_bright;
  const int cx = b.x + b.width / 2;
  const int cy = b.y + b.height / 2;
  const int s = (std::min)(b.width, b.height) / 6;
  switch (kind_) {
    case CaptionButtonKind::kMinimize:
      canvas->draw_line(cx - s, cy, cx + s, cy, fg, 1);
      break;
    case CaptionButtonKind::kMaximize:
      canvas->stroke_rect(cx - s, cy - s, s * 2, s * 2, fg, 1);
      break;
    case CaptionButtonKind::kRestore:
      canvas->stroke_rect(cx - s + 2, cy - s, s * 2 - 2, s * 2 - 2, fg, 1);
      canvas->stroke_rect(cx - s, cy - s + 2, s * 2 - 2, s * 2 - 2, fg, 1);
      break;
    case CaptionButtonKind::kClose:
      canvas->draw_line(cx - s, cy - s, cx + s, cy + s, fg, 1);
      canvas->draw_line(cx + s, cy - s, cx - s, cy + s, fg, 1);
      break;
  }
}


std::string_view CaptionButton::paint_role() const {
  return "caption_button";
}
}  // namespace views
}  // namespace ui

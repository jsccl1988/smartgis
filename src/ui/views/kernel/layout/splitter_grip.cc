// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout/splitter_grip.h"

#include <algorithm>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/kernel/shell/theme.h"

namespace ui {
namespace views {

SplitterGrip::SplitterGrip(Splitter* host) : host_(host) {
  set_preferred_size({Splitter::kBarPx, Splitter::kBarPx});
}

bool SplitterGrip::on_mouse_event(const MouseEvent& event) {
  if (!host_ || !is_enabled() || !is_visible()) {
    return false;
  }
  return host_->on_grip_mouse(event);
}

std::string_view SplitterGrip::paint_role() const {
  return "splitter";
}

void SplitterGrip::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas || !host_) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& bar = bounds();
  ui::gfx::Color fill = t.panel_header;
  if (host_->is_grip_dragging() || is_pressed()) {
    fill = t.accent;
  } else if (is_hovered()) {
    fill = t.control_hover;
  }
  canvas->fill_rect(bar.x, bar.y, bar.width, bar.height, fill);
  // Center rail: muted (not a full-bar neon slab).
  if (host_->is_horizontal_split()) {
    const int grip = std::min(2, std::max(1, bar.width));
    canvas->fill_rect(bar.x + (bar.width - grip) / 2, bar.y + 2, grip,
                      std::max(0, bar.height - 4), t.text_muted);
  } else {
    const int grip = std::min(2, std::max(1, bar.height));
    canvas->fill_rect(bar.x + 2, bar.y + (bar.height - grip) / 2,
                      std::max(0, bar.width - 4), grip, t.text_muted);
  }
}

}  // namespace views
}  // namespace ui

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/factory/placeholder_view.h"

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/theme.h"

namespace ui {
namespace views {

PlaceholderView::PlaceholderView(std::string caption)
    : caption_(std::move(caption)) {
  set_preferred_size({160, 48});
}

void PlaceholderView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  canvas->stroke_rect(b.x, b.y, b.width, b.height, t.accent, 1);
  if (!caption_.empty() && b.width > 8 && b.height > 8) {
    const std::wstring wide = utf8_to_wide(caption_);
    canvas->draw_text(b.x + 8, b.y + 8, wide.c_str(), t.text_muted);
  }
}

}  // namespace views
}  // namespace ui

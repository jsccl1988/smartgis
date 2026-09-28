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
  const int w = bounds().width;
  const int h = bounds().height;
  canvas->fill_rect(0, 0, w, h, t.panel_bg);
  canvas->stroke_rect(0, 0, w, h, t.accent, 1);
  const std::wstring wide = utf8_to_wide(caption_);
  canvas->draw_text(8, 8, wide.c_str(), t.text_muted);
}

}  // namespace views
}  // namespace ui

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/inspector_group_bar.h"

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/kernel/shell/theme.h"

namespace app {
namespace {

constexpr int kGroupCount = static_cast<int>(InspectorGroupBar::Group::kCount);
constexpr const char* kLabels[kGroupCount] = {
    "Inspect",
    "Map",
    "Process",
    "Extend",
};

float view_scale(const ui::views::View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

}  // namespace

InspectorGroupBar::InspectorGroupBar() {
  set_preferred_size({320, 26});
}

void InspectorGroupBar::set_selected(Group group) {
  if (selected_ == group) {
    return;
  }
  selected_ = group;
  schedule_paint();
}

void InspectorGroupBar::set_change(std::function<void(Group)> fn) {
  change_ = std::move(fn);
}

bool InspectorGroupBar::on_mouse_event(const ui::views::MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type != ui::views::MouseEvent::Type::kDown || e.button != 1) {
    return ui::views::View::on_mouse_event(e);
  }
  const ui::views::Rect& b = bounds();
  if (b.width <= 0 || e.y < b.y || e.y >= b.y + b.height || e.x < b.x ||
      e.x >= b.x + b.width) {
    return false;
  }
  int index = ((e.x - b.x) * kGroupCount) / b.width;
  if (index < 0) {
    index = 0;
  }
  if (index >= kGroupCount) {
    index = kGroupCount - 1;
  }
  const Group group = static_cast<Group>(index);
  set_selected(group);
  if (change_) {
    change_(group);
  }
  return true;
}

void InspectorGroupBar::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const ui::views::Theme& theme = ui::views::Theme::current();
  const ui::views::Rect& b = bounds();
  const float scale = view_scale(this);
  canvas->fill_rect(b.x, b.y, b.width, b.height, theme.panel_header);
  canvas->fill_rect(b.x, b.y + b.height - 1, b.width, 1, theme.control_border);
  if (b.width <= 0 || b.height <= 0) {
    return;
  }
  const int accent_bar = std::max(2, ui::views::dip_to_px(2, scale));
  const ui::gfx::Color accent_label = ui::gfx::color_rgb(255, 255, 255);
  for (int i = 0; i < kGroupCount; ++i) {
    const int x0 = b.x + (b.width * i) / kGroupCount;
    const int x1 = b.x + (b.width * (i + 1)) / kGroupCount;
    const int w = x1 - x0;
    const bool on = static_cast<int>(selected_) == i;
    if (on) {
      canvas->fill_rect(x0, b.y, w, b.height, theme.control_press);
      canvas->fill_rect(x0, b.y + b.height - accent_bar, w, accent_bar,
                        theme.accent);
    }
    canvas->save();
    canvas->clip_rect(x0, b.y, w, b.height);
    const ui::views::Size text = ui::views::measure_text_utf8(kLabels[i], scale);
    const int text_x = x0 + std::max(0, (w - text.width) / 2);
    const int text_y = b.y + std::max(0, (b.height - text.height) / 2);
    canvas->draw_text(text_x, text_y,
                      ui::views::utf8_to_wide(kLabels[i]).c_str(),
                      on ? accent_label : theme.text_bright);
    canvas->restore();
  }
}

std::string_view InspectorGroupBar::paint_role() const {
  return "inspector_group_bar";
}

}  // namespace app

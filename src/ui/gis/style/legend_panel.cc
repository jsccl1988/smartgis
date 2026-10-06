// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/style/legend_panel.h"

#include <memory>
#include <utility>

#include "ui/gis/scroll_table.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

LegendPanel::LegendPanel() {
  MarkupRoot loaded = load_markup("style/legend_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 180});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  table_ = loaded.ids.find_as<TableView>("table");
  if (table_) {
    table_->set_row_click([this](int row) { on_row_click(row); });
    scroll_ = wrap_markup_table_in_scroll(table_);
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 180});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 180});
}

LegendPanel::~LegendPanel() {
  if (table_) {
    table_->set_row_click({});
  }
  remove_all_children();
  title_ = nullptr;
  table_ = nullptr;
  scroll_ = nullptr;
}

void LegendPanel::set_entries(std::vector<Entry> entries) {
  entries_ = std::move(entries);
  rebuild_table();
}

void LegendPanel::set_toggle(ToggleFn fn) {
  toggle_ = std::move(fn);
}

void LegendPanel::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  for (const auto& e : entries_) {
    table_->add_row({e.visible ? "Y" : "N", e.label, e.swatch});
  }
  sync_scroll_table_content(table_, scroll_);
}

void LegendPanel::on_row_click(int row) {
  if (row < 0 || static_cast<size_t>(row) >= entries_.size()) {
    return;
  }
  entries_[static_cast<size_t>(row)].visible =
      !entries_[static_cast<size_t>(row)].visible;
  rebuild_table();
  if (toggle_) {
    toggle_(entries_[static_cast<size_t>(row)].id,
            entries_[static_cast<size_t>(row)].visible);
  }
}

void LegendPanel::on_device_scale_factor_changed(float old_scale,
                                                float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(180, s)});
}

void LegendPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui

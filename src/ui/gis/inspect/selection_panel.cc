// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/inspect/selection_panel.h"

#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

SelectionPanel::SelectionPanel() {
  MarkupRoot loaded = load_markup("inspect/selection_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 200});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  count_label_ = loaded.ids.find_as<Label>("count");
  table_ = loaded.ids.find_as<TableView>("table");
  clear_ = loaded.ids.find_as<Button>("clear");
  invert_ = loaded.ids.find_as<Button>("invert");
  zoom_ = loaded.ids.find_as<Button>("zoom");
  export_ = loaded.ids.find_as<Button>("export");

  if (clear_) {
    clear_->set_click([this]() { fire("selection.clear"); });
  }
  if (invert_) {
    invert_->set_click([this]() { fire("selection.invert"); });
  }
  if (zoom_) {
    zoom_->set_click([this]() { fire("selection.zoom_to"); });
  }
  if (export_) {
    export_->set_click([this]() { fire("selection.export_selected"); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 200});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 200});
}

SelectionPanel::~SelectionPanel() {
  if (clear_) {
    clear_->set_click({});
  }
  if (invert_) {
    invert_->set_click({});
  }
  if (zoom_) {
    zoom_->set_click({});
  }
  if (export_) {
    export_->set_click({});
  }
  remove_all_children();
  title_ = nullptr;
  count_label_ = nullptr;
  table_ = nullptr;
  clear_ = nullptr;
  invert_ = nullptr;
  zoom_ = nullptr;
  export_ = nullptr;
}

void SelectionPanel::set_count(int total) {
  count_ = total < 0 ? 0 : total;
  refresh_count_label();
}

void SelectionPanel::set_layers(std::vector<LayerSummary> layers) {
  layers_ = std::move(layers);
  rebuild_table();
}

void SelectionPanel::set_command(Command fn) {
  command_ = std::move(fn);
}

void SelectionPanel::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  for (const auto& layer : layers_) {
    table_->add_row({layer.label.empty() ? layer.layer_id : layer.label,
                     std::to_string(layer.count)});
  }
}

void SelectionPanel::fire(const std::string& id) {
  if (command_) {
    command_(id);
  }
}

void SelectionPanel::refresh_count_label() {
  if (count_label_) {
    count_label_->set_text(std::string("Selected: ") + std::to_string(count_));
  }
}

void SelectionPanel::on_device_scale_factor_changed(float old_scale,
                                                   float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(200, s)});
}

void SelectionPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui

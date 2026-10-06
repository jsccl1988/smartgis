// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/style/symbology_panel.h"

#include <memory>
#include <utility>

#include "ui/gis/scroll_table.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/markup/loader/markup_loader.h"

namespace ui {
namespace views {

namespace {

const std::string kEmpty;

}  // namespace

SymbologyPanel::SymbologyPanel() {
  MarkupRoot loaded = load_markup("style/symbology_panel.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 240});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  layer_label_ = loaded.ids.find_as<Label>("layer");
  field_ = loaded.ids.find_as<Combobox>("field");
  paint_table_ = loaded.ids.find_as<TableView>("paint");
  value_edit_ = loaded.ids.find_as<Textfield>("value");
  apply_ = loaded.ids.find_as<Button>("apply");

  if (paint_table_) {
    paint_table_->set_row_click([this](int row) {
      selected_paint_row_ = row;
      if (row >= 0 && static_cast<size_t>(row) < paint_.size() && value_edit_) {
        value_edit_->set_text(paint_[static_cast<size_t>(row)].second);
      }
    });
    paint_scroll_ = wrap_markup_table_in_scroll(paint_table_);
  }
  if (value_edit_) {
    value_edit_->set_submit([this]() { on_value_commit(); });
  }
  if (apply_) {
    apply_->set_click([this]() { on_apply(); });
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 240});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 240});
}

SymbologyPanel::~SymbologyPanel() {
  if (paint_table_) {
    paint_table_->set_row_click({});
  }
  if (value_edit_) {
    value_edit_->set_submit({});
  }
  if (apply_) {
    apply_->set_click({});
  }
  remove_all_children();
  title_ = nullptr;
  layer_label_ = nullptr;
  field_ = nullptr;
  paint_table_ = nullptr;
  paint_scroll_ = nullptr;
  value_edit_ = nullptr;
  apply_ = nullptr;
}

void SymbologyPanel::set_layer(std::string layer_token, std::string geom_type) {
  layer_token_ = std::move(layer_token);
  geom_type_ = std::move(geom_type);
  refresh_layer_label();
}

void SymbologyPanel::set_fields(std::vector<std::string> fields) {
  if (!field_ || field_->item_count() > 0) {
    return;
  }
  for (auto& f : fields) {
    field_->add_item(std::move(f));
  }
  if (field_->item_count() > 0) {
    field_->set_selected_index(0);
  }
}

const std::string& SymbologyPanel::selected_field() const {
  if (!field_ || field_->selected_index() < 0) {
    return kEmpty;
  }
  return field_->selected_text();
}

void SymbologyPanel::set_paint(PaintKv paint) {
  paint_ = std::move(paint);
  selected_paint_row_ = paint_.empty() ? -1 : 0;
  rebuild_paint_table();
  if (selected_paint_row_ >= 0 && value_edit_) {
    value_edit_->set_text(
        paint_[static_cast<size_t>(selected_paint_row_)].second);
  }
}

void SymbologyPanel::set_apply_handler(ApplyFn fn) {
  apply_handler_ = std::move(fn);
}

void SymbologyPanel::rebuild_paint_table() {
  if (!paint_table_) {
    return;
  }
  paint_table_->clear_rows();
  for (const auto& kv : paint_) {
    paint_table_->add_row({kv.first, kv.second});
  }
  if (selected_paint_row_ >= 0) {
    paint_table_->set_selected_row(selected_paint_row_);
  }
  sync_scroll_table_content(paint_table_, paint_scroll_);
}

void SymbologyPanel::refresh_layer_label() {
  if (!layer_label_) {
    return;
  }
  std::string text = "Layer: ";
  text += layer_token_.empty() ? "(none)" : layer_token_;
  if (!geom_type_.empty()) {
    text += " [";
    text += geom_type_;
    text += "]";
  }
  layer_label_->set_text(std::move(text));
}

void SymbologyPanel::on_value_commit() {
  if (selected_paint_row_ < 0 ||
      static_cast<size_t>(selected_paint_row_) >= paint_.size() ||
      !value_edit_) {
    return;
  }
  paint_[static_cast<size_t>(selected_paint_row_)].second = value_edit_->text();
  rebuild_paint_table();
}

void SymbologyPanel::on_apply() {
  on_value_commit();
  if (apply_handler_) {
    apply_handler_(paint_);
  }
}

void SymbologyPanel::on_device_scale_factor_changed(float old_scale,
                                                   float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(280, s), dip_to_px(240, s)});
}

void SymbologyPanel::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui

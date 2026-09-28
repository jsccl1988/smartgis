// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/inspect/feature_info.h"

#include <memory>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {

FeatureInfo::FeatureInfo() {
  MarkupRoot loaded = load_markup("inspect/feature_info.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({280, 180});
    return;
  }
  id_label_ = loaded.ids.find_as<Label>("id");
  table_ = loaded.ids.find_as<TableView>("table");

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({280, 180});
  add_child(std::move(loaded.root));
  set_preferred_size({280, 180});
  // Markup may ship Name/Value columns with zero rows; seed the empty-state
  // row so the panel does not look like a frozen blank table.
  rebuild_table();
}

void FeatureInfo::set_feature_id(std::string id) {
  feature_id_ = std::move(id);
  if (id_label_) {
    id_label_->set_text(feature_id_.empty() ? "Feature" : feature_id_);
  }
}

void FeatureInfo::set_fields(const std::vector<Field>& fields) {
  fields_ = fields;
  field_count_ = fields_.size();
  rebuild_table();
}

void FeatureInfo::clear() {
  feature_id_.clear();
  fields_.clear();
  field_count_ = 0;
  if (id_label_) {
    id_label_->set_text("Feature");
  }
  rebuild_table();
}

void FeatureInfo::rebuild_table() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  table_->set_columns({"Name", "Value"});
  if (fields_.empty()) {
    table_->add_row({"(no fields)", ""});
  } else {
    for (const auto& field : fields_) {
      table_->add_row({field.name, field.value});
    }
  }
  const int width =
      bounds().width > 0 ? bounds().width : preferred_size().width;
  const int height = table_->header_height() +
                     static_cast<int>(table_->row_count()) * table_->row_height();
  table_->set_preferred_size({width, height});
}

void FeatureInfo::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.shell_bg);
}

}  // namespace views
}  // namespace ui

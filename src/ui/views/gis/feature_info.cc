// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/gis/feature_info.h"

#include <memory>
#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/primitives/label.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/primitives/table_view.h"
#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {

FeatureInfo::FeatureInfo() {
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  auto id = std::make_unique<Label>("Feature");
  id->set_preferred_size({240, 24});
  id_label_ = id.get();
  auto table = std::make_unique<TableView>();
  table->set_columns({"Name", "Value"});
  table_ = table.get();
  box->set_flex_for_view(table_, 1);
  set_layout_manager(std::move(box));
  add_child(std::move(id));
  add_child(std::move(table));
  set_preferred_size({280, 180});
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

void FeatureInfo::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.chrome_bg);
}

}  // namespace views
}  // namespace ui

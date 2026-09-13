// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/attribute_table.h"

#include <memory>
#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/layout.h"
#include "ui/views/scroll_view.h"
#include "ui/views/table_view.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

namespace {

constexpr int kHeaderH = 24;
constexpr int kRowH = 20;

}  // namespace

AttributeTable::AttributeTable() {
  auto table = std::make_unique<TableView>();
  table_ = table.get();
  table_->set_row_click([this](int row) {
    if (selected_) {
      selected_(row);
    }
  });
  auto scroll = std::make_unique<ScrollView>();
  scroll->set_layout_manager(std::make_unique<FillLayout>());
  scroll->add_child(std::move(table));
  scroll_ = scroll.get();
  set_layout_manager(std::make_unique<FillLayout>());
  add_child(std::move(scroll));
  set_preferred_size({320, 160});
}

void AttributeTable::set_columns(const std::vector<std::string>& cols) {
  if (table_) {
    table_->set_columns(cols);
  }
  notify_changed();
  sync_content_size();
}

void AttributeTable::set_rows(
    const std::vector<std::vector<std::string>>& rows) {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  for (const auto& row : rows) {
    table_->add_row(row);
  }
  notify_changed();
  sync_content_size();
}

void AttributeTable::add_row(const std::vector<std::string>& cells) {
  if (table_) {
    table_->add_row(cells);
  }
  notify_changed();
  sync_content_size();
}

void AttributeTable::clear() {
  if (table_) {
    table_->clear_rows();
  }
  notify_changed();
  sync_content_size();
}

int AttributeTable::selected_row() const {
  return table_ ? table_->selected_row() : -1;
}

int AttributeTable::row_count() const {
  return table_ ? static_cast<int>(table_->row_count()) : 0;
}

bool AttributeTable::on_mouse_event(const MouseEvent& event) {
  layout();
  return View::on_mouse_event(event);
}

void AttributeTable::set_changed(Changed fn) {
  changed_ = std::move(fn);
}

void AttributeTable::set_selected(Selected fn) {
  selected_ = std::move(fn);
}

void AttributeTable::notify_changed() {
  if (changed_) {
    changed_();
  }
}

void AttributeTable::sync_content_size() {
  if (!table_) {
    return;
  }
  const int width = bounds().width > 0 ? bounds().width
                                       : preferred_size().width;
  const int height =
      kHeaderH + static_cast<int>(table_->row_count()) * kRowH;
  table_->set_preferred_size({width, height});
  if (scroll_) {
    scroll_->layout();
  }
}

void AttributeTable::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
}

}  // namespace views
}  // namespace ui

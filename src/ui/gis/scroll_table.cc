// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/scroll_table.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ui/views/kernel/view/view.h"
#include "ui/views/markup/layout/yoga_layout_manager.h"
#include "ui/views/markup/style/flex_style.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/collection/table_view.h"

namespace ui {
namespace views {

ScrollView* wrap_markup_table_in_scroll(TableView* table,
                                        float min_height_dip) {
  if (!table) {
    return nullptr;
  }
  View* parent = table->parent();
  if (!parent) {
    return nullptr;
  }
  auto* yoga = dynamic_cast<YogaLayoutManager*>(parent->layout_manager());
  std::unique_ptr<View> owned = parent->remove_child(table);
  auto scroll = std::make_unique<ScrollView>();
  ScrollView* scroll_ptr = scroll.get();
  scroll_ptr->add_child(std::move(owned));
  if (yoga) {
    yoga->clear_child_style(table);
    FlexStyle grow;
    grow.flex_grow = 1.f;
    if (min_height_dip > 0.f) {
      grow.min_height = min_height_dip;
    }
    yoga->set_child_style(scroll_ptr, grow);
  }
  parent->add_child(std::move(scroll));
  return scroll_ptr;
}

void sync_scroll_table_content(TableView* table,
                               ScrollView* scroll,
                               int width_hint) {
  if (!table) {
    return;
  }
  int width = width_hint;
  if (width <= 0 && scroll && scroll->bounds().width > 0) {
    width = scroll->bounds().width;
  }
  if (width <= 0 && table->bounds().width > 0) {
    width = table->bounds().width;
  }
  if (width <= 0) {
    width = table->preferred_size().width;
  }
  const int height = table->header_height() +
                     static_cast<int>(table->row_count()) * table->row_height();
  const Size next{std::max(0, width), std::max(0, height)};
  if (table->preferred_size().width != next.width ||
      table->preferred_size().height != next.height) {
    table->set_preferred_size(next);
  }
  if (scroll) {
    scroll->layout();
  }
}

}  // namespace views
}  // namespace ui

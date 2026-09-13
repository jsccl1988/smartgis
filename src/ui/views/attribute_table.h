// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_ATTRIBUTE_TABLE_H_
#define UI_VIEWS_ATTRIBUTE_TABLE_H_

#include <functional>
#include <string>
#include <vector>

#include "ui/views/view.h"

namespace ui {
namespace views {

class ScrollView;
class TableView;

// Public GIS attribute grid. Hosts a TableView inside a ScrollView; callers
// pass string cells, never SmtFeature* or field objects from sdb.
class AttributeTable : public View {
 public:
  using Changed = std::function<void()>;
  using Selected = std::function<void(int row)>;

  AttributeTable();

  void set_columns(const std::vector<std::string>& cols);
  void set_rows(const std::vector<std::vector<std::string>>& rows);
  void add_row(const std::vector<std::string>& cells);
  void clear();

  int selected_row() const;
  int row_count() const;
  void set_changed(Changed fn);
  void set_selected(Selected fn);
  bool on_mouse_event(const MouseEvent& event) override;

  TableView* table() { return table_; }
  const TableView* table() const { return table_; }

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void notify_changed();
  void sync_content_size();

  ScrollView* scroll_ = nullptr;
  TableView* table_ = nullptr;
  Changed changed_;
  Selected selected_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_ATTRIBUTE_TABLE_H_

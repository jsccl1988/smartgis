// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_COLLECTION_TABLE_VIEW_H_
#define UI_VIEWS_PRIMITIVES_COLLECTION_TABLE_VIEW_H_

#include "ui/ui_views_export.h"
#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Columnar preview table with an optional selected row.
class UI_VIEWS_EXPORT TableView : public View {
 public:
  TableView();
  void set_columns(const std::vector<std::string>& cols);
  void add_row(const std::vector<std::string>& cells);
  void clear_rows();
  size_t row_count() const;
  const std::vector<std::string>& columns() const;
  const std::vector<std::string>& row_at(size_t i) const;
  void set_selected_row(int i);
  int selected_row() const { return selected_; }
  void set_row_click(std::function<void(int)> fn);
  // Fired on left double-click over a data cell (row, col).
  void set_cell_activate(std::function<void(int row, int col)> fn);
  bool set_cell(int row, int col, const std::string& value);
  bool on_mouse_event(const MouseEvent& e) override;

  // Row geometry in physical pixels (24 DIP each at scale 1.0).
  int header_height() const;
  int row_height() const;
  // Content-sized column width in pixels. Last column absorbs leftover width
  // so Name/Value inspectors do not split the pane 50/50.
  int column_width(int col) const;

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

  // Data rows emitted by the last paint_self. Header is not included.
  int last_painted_row_count() const { return last_painted_rows_; }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  int row_at_point(int y) const;
  int col_at_point(int x) const;
  float scale_factor() const;
  void visible_row_span(int* begin, int* end) const;

  std::vector<std::string> columns_;
  std::vector<std::wstring> column_wide_;
  std::vector<std::vector<std::string>> rows_;
  std::vector<std::vector<std::wstring>> row_wide_;
  int selected_ = -1;
  int last_painted_rows_ = 0;
  std::function<void(int)> row_click_;
  std::function<void(int, int)> cell_activate_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_COLLECTION_TABLE_VIEW_H_

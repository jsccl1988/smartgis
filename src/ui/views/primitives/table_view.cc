// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/table_view.h"

#include "render/skia/canvas.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

TableView::TableView() {
  set_preferred_size({320, 160});
  set_focusable(true);
}

void TableView::set_columns(const std::vector<std::string>& cols) {
  columns_ = cols;
  schedule_paint();
}

void TableView::add_row(const std::vector<std::string>& cells) {
  rows_.push_back(cells);
  schedule_paint();
}

void TableView::clear_rows() {
  rows_.clear();
  selected_ = -1;
  schedule_paint();
}

size_t TableView::row_count() const {
  return rows_.size();
}

const std::vector<std::string>& TableView::columns() const {
  return columns_;
}

const std::vector<std::string>& TableView::row_at(size_t i) const {
  static const std::vector<std::string> kEmpty;
  return i < rows_.size() ? rows_[i] : kEmpty;
}

void TableView::set_selected_row(int i) {
  if (i < -1 || i >= static_cast<int>(rows_.size())) {
    return;
  }
  selected_ = i;
  schedule_paint();
}

bool TableView::set_cell(int row, int col, const std::string& value) {
  if (row < 0 || row >= static_cast<int>(rows_.size()) || col < 0) {
    return false;
  }
  auto& cells = rows_[static_cast<size_t>(row)];
  if (col >= static_cast<int>(cells.size())) {
    cells.resize(static_cast<size_t>(col) + 1);
  }
  cells[static_cast<size_t>(col)] = value;
  schedule_paint();
  return true;
}

int TableView::row_at_point(int y) const {
  const Rect& b = bounds();
  if (y < b.y + header_height() || y >= b.bottom()) {
    return -1;
  }
  const int i = (y - b.y - header_height()) / row_height();
  if (i < 0 || i >= static_cast<int>(rows_.size())) {
    return -1;
  }
  return i;
}

int TableView::col_at_point(int x) const {
  const Rect& b = bounds();
  if (x < b.x || x >= b.right()) {
    return -1;
  }
  const int cols = columns_.empty() ? 1 : static_cast<int>(columns_.size());
  if (cols <= 0 || b.width <= 0) {
    return -1;
  }
  const int col_w = b.width / cols;
  if (col_w <= 0) {
    return 0;
  }
  const int c = (x - b.x) / col_w;
  if (c < 0 || c >= cols) {
    return -1;
  }
  return c;
}

bool TableView::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kDblClick && e.button == 1) {
    const int row = row_at_point(e.y);
    const int col = col_at_point(e.x);
    if (row >= 0 && col >= 0) {
      set_selected_row(row);
      if (cell_activate_) {
        cell_activate_(row, col);
      }
      return true;
    }
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    const int i = row_at_point(e.y);
    if (i >= 0) {
      set_selected_row(i);
      if (row_click_) {
        row_click_(i);
      }
      return true;
    }
  }
  return e.type == MouseEvent::Type::kDown && e.button == 1;
}

void TableView::set_row_click(std::function<void(int)> fn) {
  row_click_ = std::move(fn);
}

void TableView::set_cell_activate(std::function<void(int, int)> fn) {
  cell_activate_ = std::move(fn);
}

void TableView::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
  const int cols = columns_.empty() ? 1 : static_cast<int>(columns_.size());
  const int col_w = b.width / cols;
  canvas->fill_rect(b.x, b.y, b.width, header_height(), t.panel_header);
  for (int c = 0; c < static_cast<int>(columns_.size()); ++c) {
    const std::wstring w = utf8_to_wide(columns_[static_cast<size_t>(c)]);
    canvas->draw_text(b.x + c * col_w + 4, b.y + 4, w.c_str(), t.text_bright);
  }
  for (int r = 0; r < static_cast<int>(rows_.size()); ++r) {
    const int y = b.y + header_height() + r * row_height();
    if (y + row_height() > b.bottom()) {
      break;
    }
    if (r == selected_) {
      canvas->fill_rect(b.x, y, b.width, row_height(), t.accent);
    }
    const auto& cells = rows_[static_cast<size_t>(r)];
    const int n = static_cast<int>(cells.size());
    for (int c = 0; c < cols && c < n; ++c) {
      const std::wstring w = utf8_to_wide(cells[static_cast<size_t>(c)]);
      canvas->draw_text(b.x + c * col_w + 4, y + 3, w.c_str(), t.text);
    }
  }
  if (is_focused()) {
    draw_focus_ring(canvas, b);
  }
}

}  // namespace views
}  // namespace ui

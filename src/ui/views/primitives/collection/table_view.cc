// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/collection/table_view.h"

#include <algorithm>
#include <cstdint>

#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

constexpr int kHeaderHeightDip = 24;
constexpr int kRowHeightDip = 24;

}  // namespace

TableView::TableView() {
  set_preferred_size({320, 160});
  set_focusable(true);
}

float TableView::scale_factor() const {
  if (widget()) {
    return widget()->device_scale_factor();
  }
  return 1.f;
}

int TableView::header_height() const {
  return dip_to_px(kHeaderHeightDip, scale_factor());
}

int TableView::row_height() const {
  return dip_to_px(kRowHeightDip, scale_factor());
}

int TableView::column_width(int col) const {
  const Rect& b = bounds();
  const int cols = columns_.empty() ? 1 : static_cast<int>(columns_.size());
  if (cols <= 0 || b.width <= 0 || col < 0 || col >= cols) {
    return 0;
  }
  const int base = b.width / cols;
  if (col + 1 == cols) {
    return b.width - base * (cols - 1);
  }
  return base;
}

void TableView::invalidate_row_cache() {
  cache_valid_ = false;
  row_cache_.clear();
}

void TableView::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  invalidate_row_cache();
  schedule_paint();
}

void TableView::set_columns(const std::vector<std::string>& cols) {
  columns_ = cols;
  column_wide_.clear();
  column_wide_.reserve(cols.size());
  for (const std::string& col : cols) {
    column_wide_.push_back(utf8_to_wide(col));
  }
  invalidate_row_cache();
  invalidate_commands();
  schedule_paint();
}

void TableView::add_row(const std::vector<std::string>& cells) {
  rows_.push_back(cells);
  std::vector<std::wstring> wide;
  wide.reserve(cells.size());
  for (const std::string& cell : cells) {
    wide.push_back(utf8_to_wide(cell));
  }
  row_wide_.push_back(std::move(wide));
  invalidate_row_cache();
  invalidate_commands();
  schedule_paint();
}

void TableView::clear_rows() {
  rows_.clear();
  row_wide_.clear();
  selected_ = -1;
  invalidate_row_cache();
  invalidate_commands();
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
  invalidate_row_cache();
  invalidate_commands();
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
  auto& wide = row_wide_[static_cast<size_t>(row)];
  if (col >= static_cast<int>(wide.size())) {
    wide.resize(static_cast<size_t>(col) + 1);
  }
  wide[static_cast<size_t>(col)] = utf8_to_wide(value);
  invalidate_row_cache();
  invalidate_commands();
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

void TableView::visible_row_span(int* begin, int* end) const {
  *begin = 0;
  *end = 0;
  const int rh = row_height();
  if (rh <= 0 || rows_.empty()) {
    return;
  }
  const Rect& b = bounds();
  Rect vis = exposed_rect();
  if (vis.width <= 0 || vis.height <= 0) {
    vis = b;
  }
  const int first_y = b.y + header_height();
  const int y0 = std::max(vis.y, first_y);
  const int y1 = std::min(vis.bottom(), b.bottom());
  if (y1 <= y0) {
    return;
  }
  int first = (y0 - first_y) / rh;
  int last = (y1 - first_y) / rh;
  if ((y1 - first_y) % rh != 0) {
    ++last;
  }
  if (first < 0) {
    first = 0;
  }
  const int n = static_cast<int>(rows_.size());
  if (last > n) {
    last = n;
  }
  if (first > last) {
    first = last;
  }
  *begin = first;
  *end = last;
}

bool TableView::row_cache_matches(int begin, int end) const {
  if (!cache_valid_) {
    return false;
  }
  const Rect& b = bounds();
  if (cache_begin_ != begin || cache_end_ != end ||
      cache_selected_ != selected_ || cache_origin_x_ != b.x ||
      cache_origin_y_ != b.y || cache_width_ != b.width) {
    return false;
  }
  const Theme& t = Theme::current();
  return cache_control_bg_ == t.control_bg &&
         cache_panel_header_ == t.panel_header && cache_accent_ == t.accent &&
         cache_text_ == t.text && cache_text_bright_ == t.text_bright;
}

void TableView::rebuild_row_cache(int begin, int end) {
  LARGE_INTEGER t0 = {};
  QueryPerformanceCounter(&t0);

  row_cache_.clear();
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  row_cache_.fill_rect(b.x, b.y, b.width, b.height, t.control_bg);
  const int cols = columns_.empty() ? 1 : static_cast<int>(columns_.size());
  const int col_w = cols > 0 ? b.width / cols : b.width;
  Rect vis = exposed_rect();
  if (vis.width <= 0 || vis.height <= 0) {
    vis = b;
  }
  if (b.y < vis.bottom() && b.y + header_height() > vis.y) {
    row_cache_.fill_rect(b.x, b.y, b.width, header_height(), t.panel_header);
    for (int c = 0; c < static_cast<int>(column_wide_.size()); ++c) {
      row_cache_.draw_text(b.x + c * col_w + 4, b.y + 4,
                           column_wide_[static_cast<size_t>(c)].c_str(),
                           t.text_bright);
    }
  }
  for (int r = begin; r < end; ++r) {
    const int y = b.y + header_height() + r * row_height();
    if (r == selected_) {
      row_cache_.fill_rect(b.x, y, b.width, row_height(), t.accent);
    }
    if (r < 0 || r >= static_cast<int>(row_wide_.size())) {
      continue;
    }
    const auto& cells = row_wide_[static_cast<size_t>(r)];
    const int n = static_cast<int>(cells.size());
    for (int c = 0; c < cols && c < n; ++c) {
      row_cache_.draw_text(b.x + c * col_w + 4, y + 3,
                           cells[static_cast<size_t>(c)].c_str(), t.text);
    }
  }

  cache_begin_ = begin;
  cache_end_ = end;
  cache_selected_ = selected_;
  cache_origin_x_ = b.x;
  cache_origin_y_ = b.y;
  cache_width_ = b.width;
  cache_control_bg_ = t.control_bg;
  cache_panel_header_ = t.panel_header;
  cache_accent_ = t.accent;
  cache_text_ = t.text;
  cache_text_bright_ = t.text_bright;
  cache_valid_ = true;

  LARGE_INTEGER t1 = {};
  QueryPerformanceCounter(&t1);
  ui::gfx::note_table_scroll_qpc(
      static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
}

void TableView::emit_row_cache(ui::gfx::Canvas* canvas) {
  // DisplayList::replay disables the thread_local recorder; while
  // ensure_commands_recorded is active, append into the live list instead.
  if (ui::gfx::DisplayList* rec = ui::gfx::display_list_recorder()) {
    rec->append_from(row_cache_);
    return;
  }
  if (!canvas) {
    return;
  }
  Rect vis = exposed_rect();
  if (vis.width <= 0 || vis.height <= 0) {
    vis = bounds();
  }
  row_cache_.replay_clipped(canvas, vis.x, vis.y, vis.right(), vis.bottom());
}

void TableView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  int begin = 0;
  int end = 0;
  visible_row_span(&begin, &end);
  last_painted_rows_ = end - begin;

  if (row_cache_matches(begin, end)) {
    last_cache_hit_ = true;
  } else {
    last_cache_hit_ = false;
    rebuild_row_cache(begin, end);
  }
  emit_row_cache(canvas);

  if (is_focused()) {
    draw_focus_ring(canvas, bounds());
  }
}

std::string_view TableView::paint_role() const {
  return "table_view";
}

}  // namespace views
}  // namespace ui

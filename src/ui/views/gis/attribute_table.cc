// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/gis/attribute_table.h"

#include <memory>
#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/primitives/scroll_view.h"
#include "ui/views/primitives/table_view.h"
#include "ui/views/kernel/theme.h"

namespace ui {
namespace views {

namespace {

const std::string& empty_token() {
  static const std::string kEmpty;
  return kEmpty;
}

}  // namespace

AttributeTable::AttributeTable() {
  set_focusable(true);
  auto table = std::make_unique<TableView>();
  table_ = table.get();
  table_->set_row_click([this](int row) {
    if (selected_) {
      selected_(row);
    }
  });
  table_->set_cell_activate(
      [this](int row, int col) { on_cell_activate(row, col); });
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
  if (editing_) {
    cancel_edit();
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
  if (editing_) {
    cancel_edit();
  }
  row_tokens_.clear();
  if (table_) {
    table_->clear_rows();
  }
  notify_changed();
  sync_content_size();
}

void AttributeTable::set_row_tokens(std::vector<std::string> tokens) {
  row_tokens_ = std::move(tokens);
}

const std::string& AttributeTable::row_token(int row) const {
  if (row < 0 || row >= static_cast<int>(row_tokens_.size())) {
    return empty_token();
  }
  return row_tokens_[static_cast<size_t>(row)];
}

void AttributeTable::set_on_cell_commit(CellCommit fn) {
  on_cell_commit_ = std::move(fn);
}

bool AttributeTable::commit_cell(int row, int col, const std::string& value) {
  if (!table_ || row < 0 || col < 0 ||
      row >= static_cast<int>(table_->row_count())) {
    return false;
  }
  const auto& cols = table_->columns();
  if (col >= static_cast<int>(cols.size())) {
    return false;
  }
  const std::string& token = row_token(row);
  if (token.empty()) {
    return false;
  }
  const std::string& field = cols[static_cast<size_t>(col)];
  std::string previous;
  const auto& cells = table_->row_at(static_cast<size_t>(row));
  if (col < static_cast<int>(cells.size())) {
    previous = cells[static_cast<size_t>(col)];
  }
  if (!table_->set_cell(row, col, value)) {
    return false;
  }
  if (on_cell_commit_ && !on_cell_commit_(token, field, value)) {
    table_->set_cell(row, col, previous);
    return false;
  }
  notify_changed();
  return true;
}

bool AttributeTable::commit_cells(const std::vector<CellEdit>& edits) {
  for (const auto& edit : edits) {
    if (!commit_cell(edit.row, edit.col, edit.value)) {
      return false;
    }
  }
  return true;
}

bool AttributeTable::begin_edit(int row, int col) {
  if (!table_ || row < 0 || col < 0 ||
      row >= static_cast<int>(table_->row_count()) ||
      col >= static_cast<int>(table_->columns().size()) ||
      row_token(row).empty()) {
    return false;
  }
  if (editing_) {
    if (edit_row_ == row && edit_col_ == col) {
      request_focus();
      return true;
    }
    if (!finish_edit(true)) {
      cancel_edit();
    }
  }
  edit_row_ = row;
  edit_col_ = col;
  edit_original_.clear();
  const auto& cells = table_->row_at(static_cast<size_t>(row));
  if (col < static_cast<int>(cells.size())) {
    edit_original_ = cells[static_cast<size_t>(col)];
  }
  edit_buffer_ = edit_original_;
  editing_ = true;
  table_->set_selected_row(row);
  request_focus();
  schedule_paint();
  return true;
}

void AttributeTable::cancel_edit() {
  if (!editing_) {
    return;
  }
  if (table_) {
    table_->set_cell(edit_row_, edit_col_, edit_original_);
  }
  editing_ = false;
  edit_row_ = -1;
  edit_col_ = -1;
  edit_original_.clear();
  edit_buffer_.clear();
  schedule_paint();
}

bool AttributeTable::finish_edit(bool commit) {
  if (!editing_) {
    return false;
  }
  const int row = edit_row_;
  const int col = edit_col_;
  const std::string value = edit_buffer_;
  const std::string original = edit_original_;
  editing_ = false;
  edit_row_ = -1;
  edit_col_ = -1;
  edit_original_.clear();
  edit_buffer_.clear();
  if (!commit) {
    if (table_) {
      table_->set_cell(row, col, original);
    }
    schedule_paint();
    return true;
  }
  // Restore original first so commit_cell can revert correctly on host reject.
  if (table_) {
    table_->set_cell(row, col, original);
  }
  return commit_cell(row, col, value);
}

bool AttributeTable::delete_edit_last_char() {
  if (!editing_ || edit_buffer_.empty()) {
    return editing_;
  }
  size_t i = edit_buffer_.size();
  do {
    --i;
  } while (i > 0 &&
           (static_cast<unsigned char>(edit_buffer_[i]) & 0xC0) == 0x80);
  edit_buffer_.resize(i);
  if (table_) {
    table_->set_cell(edit_row_, edit_col_, edit_buffer_);
  }
  schedule_paint();
  return true;
}

void AttributeTable::on_cell_activate(int row, int col) {
  begin_edit(row, col);
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

bool AttributeTable::on_key_event(const KeyEvent& event) {
  if (editing_ && event.type == KeyEvent::Type::kDown) {
    if (event.vk == VK_RETURN) {
      return finish_edit(true);
    }
    if (event.vk == VK_ESCAPE) {
      cancel_edit();
      return true;
    }
    if (event.vk == VK_BACK) {
      return delete_edit_last_char();
    }
  }
  return View::on_key_event(event);
}

bool AttributeTable::on_char_event(const CharEvent& event) {
  if (!editing_) {
    return View::on_char_event(event);
  }
  if (event.ch == L'\b') {
    return delete_edit_last_char();
  }
  if (event.ch < 32 || event.ch == 127) {
    return false;
  }
  const wchar_t buf[2] = {event.ch, 0};
  edit_buffer_ += wide_to_utf8(buf);
  if (table_) {
    table_->set_cell(edit_row_, edit_col_, edit_buffer_);
  }
  schedule_paint();
  return true;
}

void AttributeTable::on_blur() {
  if (editing_) {
    finish_edit(true);
  }
  View::on_blur();
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
  // Match TableView DIP→px metrics so HiDPI GDI chrome text does not crowd.
  const int height = table_->header_height() +
                     static_cast<int>(table_->row_count()) * table_->row_height();
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

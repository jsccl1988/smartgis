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
// pass string cells and opaque feature tokens, never SmtFeature* or sdb field
// objects. Cell commits go through on_cell_commit(token, field, value).
class AttributeTable : public View {
 public:
  using Changed = std::function<void()>;
  using Selected = std::function<void(int row)>;
  // Host writeback. Return false to reject; the table reverts the cell.
  using CellCommit = std::function<bool(const std::string& feature_token,
                                        const std::string& field,
                                        const std::string& value)>;

  struct CellEdit {
    int row = 0;
    int col = 0;
    std::string value;
  };

  AttributeTable();

  void set_columns(const std::vector<std::string>& cols);
  void set_rows(const std::vector<std::vector<std::string>>& rows);
  void add_row(const std::vector<std::string>& cells);
  void clear();

  // Opaque per-row feature tokens parallel to data rows (string ids only).
  void set_row_tokens(std::vector<std::string> tokens);
  const std::string& row_token(int row) const;

  void set_on_cell_commit(CellCommit fn);

  // Programmatic write: update cell then invoke on_cell_commit.
  bool commit_cell(int row, int col, const std::string& value);
  // Batch write; stops on first host rejection.
  bool commit_cells(const std::vector<CellEdit>& edits);

  // In-place edit: type chars, Enter/blur commits, Escape cancels.
  bool begin_edit(int row, int col);
  void cancel_edit();
  bool is_editing() const { return editing_; }

  int selected_row() const;
  int row_count() const;
  void set_changed(Changed fn);
  void set_selected(Selected fn);
  bool on_mouse_event(const MouseEvent& event) override;
  bool on_key_event(const KeyEvent& event) override;
  bool on_char_event(const CharEvent& event) override;
  void on_blur() override;

  TableView* table() { return table_; }
  const TableView* table() const { return table_; }

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void notify_changed();
  void sync_content_size();
  bool finish_edit(bool commit);
  bool delete_edit_last_char();
  void on_cell_activate(int row, int col);

  ScrollView* scroll_ = nullptr;
  TableView* table_ = nullptr;
  Changed changed_;
  Selected selected_;
  CellCommit on_cell_commit_;
  std::vector<std::string> row_tokens_;

  bool editing_ = false;
  int edit_row_ = -1;
  int edit_col_ = -1;
  std::string edit_original_;
  std::string edit_buffer_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_ATTRIBUTE_TABLE_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_COLLECTION_TABLE_VIEW_H_
#define UI_VIEWS_PRIMITIVES_COLLECTION_TABLE_VIEW_H_

#include "ui/ui_export.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/gfx/color/color.h"
#include "ui/gfx/display_list/display_list.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Columnar preview table with an optional selected row.
// Paint records only the exposed_rect row strip (plus header if visible) so
// scroll commit does not fill or stroke the offscreen body. Overlapping rows
// reuse per-row DisplayList slots keyed by content Y. Hit-test still uses
// full content geometry.
class UI_EXPORT TableView : public View {
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
  // True when the last paint_self reused row_cache_ without rebuilding.
  bool last_cache_hit() const { return last_cache_hit_; }
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  int row_at_point(int y) const;
  int col_at_point(int x) const;
  float scale_factor() const;
  Rect visible_clip_rect() const;
  void visible_row_span(int* begin, int* end) const;
  void invalidate_row_cache();
  void paint_row_strip(ui::gfx::DisplayList* dl, int row, int y) const;
  const ui::gfx::DisplayList* cached_row_strip(int row, int y);
  void rebuild_row_cache(int begin, int end);
  void emit_row_cache(ui::gfx::Canvas* canvas);
  bool row_cache_matches(int begin, int end) const;

  std::vector<std::string> columns_;
  std::vector<std::wstring> column_wide_;
  std::vector<std::vector<std::string>> rows_;
  std::vector<std::vector<std::wstring>> row_wide_;
  int selected_ = -1;
  int hovered_ = -1;
  int last_painted_rows_ = 0;
  bool last_cache_hit_ = false;
  std::function<void(int)> row_click_;
  std::function<void(int, int)> cell_activate_;

  // Cached paint for the current visible_row_span (+ header) and clip strip.
  int cache_begin_ = 0;
  int cache_end_ = 0;
  int cache_selected_ = -1;
  int cache_hovered_ = -1;
  int cache_origin_x_ = 0;
  int cache_origin_y_ = 0;
  int cache_width_ = 0;
  int cache_clip_x_ = 0;
  int cache_clip_y_ = 0;
  int cache_clip_w_ = 0;
  int cache_clip_h_ = 0;
  ui::gfx::Color cache_control_bg_ = 0;
  ui::gfx::Color cache_panel_header_ = 0;
  ui::gfx::Color cache_accent_ = 0;
  ui::gfx::Color cache_text_ = 0;
  ui::gfx::Color cache_text_bright_ = 0;
  ui::gfx::Color cache_text_muted_ = 0;
  ui::gfx::Color cache_row_alt_ = 0;
  ui::gfx::Color cache_control_border_ = 0;
  ui::gfx::Color cache_control_hover_ = 0;
  bool cache_valid_ = false;
  ui::gfx::DisplayList row_cache_;

  // Sliding per-row command strips; heap type lives in the .cc.
  std::unique_ptr<void, void (*)(void*)> row_strips_{nullptr, nullptr};
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_COLLECTION_TABLE_VIEW_H_

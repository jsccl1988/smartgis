// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_STYLE_LEGEND_PANEL_H_
#define UI_GIS_STYLE_LEGEND_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Label;
class ScrollView;
class TableView;

// Read-only legend from host-serialized snapshot rows (label + swatch text).
class UI_EXPORT LegendPanel : public View {
 public:
  struct Entry {
    std::string id;
    std::string label;
    std::string swatch;
    bool visible = true;
  };

  using ToggleFn = std::function<void(const std::string& id, bool visible)>;

  LegendPanel();
  ~LegendPanel() override;

  void set_entries(std::vector<Entry> entries);
  size_t entry_count() const { return entries_.size(); }

  void set_toggle(ToggleFn fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_table();
  void on_row_click(int row);

  Label* title_ = nullptr;
  TableView* table_ = nullptr;
  ScrollView* scroll_ = nullptr;
  std::vector<Entry> entries_;
  ToggleFn toggle_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_STYLE_LEGEND_PANEL_H_

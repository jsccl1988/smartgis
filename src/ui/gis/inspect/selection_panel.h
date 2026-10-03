// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_INSPECT_SELECTION_PANEL_H_
#define UI_GIS_INSPECT_SELECTION_PANEL_H_

#include "ui/ui_export.h"

#include <functional>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Button;
class Label;
class TableView;

// Selection-set manager: count + per-layer summary + command buttons.
// Hosts own feature tokens; this panel fires opaque command ids only.
class UI_EXPORT SelectionPanel : public View {
 public:
  struct LayerSummary {
    std::string layer_id;
    std::string label;
    int count = 0;
  };

  using Command = std::function<void(const std::string& command_id)>;

  SelectionPanel();
  ~SelectionPanel() override;

  void set_count(int total);
  int count() const { return count_; }

  void set_layers(std::vector<LayerSummary> layers);
  size_t layer_count() const { return layers_.size(); }

  void set_command(Command fn);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_table();
  void fire(const std::string& id);
  void refresh_count_label();
  void sync_commands_enabled();

  Label* title_ = nullptr;
  Label* count_label_ = nullptr;
  TableView* table_ = nullptr;
  Button* clear_ = nullptr;
  Button* invert_ = nullptr;
  Button* zoom_ = nullptr;
  Button* export_ = nullptr;

  int count_ = 0;
  std::vector<LayerSummary> layers_;
  Command command_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_GIS_INSPECT_SELECTION_PANEL_H_

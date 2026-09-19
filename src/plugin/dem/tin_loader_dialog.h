// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_DEM_TIN_LOADER_DIALOG_H_
#define PLUGIN_DEM_TIN_LOADER_DIALOG_H_

#include <string>

#include "ui/views/kernel/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Checkbox;
class Combobox;
class RadioButton;
class TableView;
class Textfield;
}

namespace plugin {

// Views dialog for ASCII XYZ point files 鈫?DEM TIN processing.
class TinLoaderDialog : public ui::views::View {
 public:
  explicit TinLoaderDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_pick_vertex();
  void on_pick_texture();
  void update_preview();
  std::string separator_name() const;
  std::string build_json() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* vertex_path_ = nullptr;
  ui::views::RadioButton* radio_tab_ = nullptr;
  ui::views::RadioButton* radio_space_ = nullptr;
  ui::views::RadioButton* radio_comma_ = nullptr;
  ui::views::Textfield* head_skip_ = nullptr;
  ui::views::Textfield* line_skip_ = nullptr;
  ui::views::Combobox* col_x_ = nullptr;
  ui::views::Combobox* col_y_ = nullptr;
  ui::views::Combobox* col_z_ = nullptr;
  ui::views::Textfield* x_scale_ = nullptr;
  ui::views::Textfield* y_scale_ = nullptr;
  ui::views::Textfield* z_scale_ = nullptr;
  ui::views::Checkbox* use_texture_ = nullptr;
  ui::views::Textfield* texture_path_ = nullptr;
  ui::views::Checkbox* gen_2d_tin_ = nullptr;
  ui::views::Combobox* tin_layer_ = nullptr;
  ui::views::TableView* preview_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_DEM_TIN_LOADER_DIALOG_H_

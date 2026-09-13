// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_DEM_GRID_LOADER_DIALOG_H_
#define PLUGIN_DEM_GRID_LOADER_DIALOG_H_

#include <string>

#include "ui/views/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Checkbox;
class Combobox;
class Textfield;
}

namespace plugin {

// Views dialog for height-map rasters → DEM grid processing.
class GridLoaderDialog : public ui::views::View {
 public:
  explicit GridLoaderDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_pick_heightmap();
  void on_pick_texture();
  std::string build_json() const;
  int color_type_value() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* heightmap_path_ = nullptr;
  ui::views::Textfield* x_scale_ = nullptr;
  ui::views::Textfield* y_scale_ = nullptr;
  ui::views::Textfield* z_scale_ = nullptr;
  ui::views::Textfield* x_start_ = nullptr;
  ui::views::Textfield* y_start_ = nullptr;
  ui::views::Textfield* z_start_ = nullptr;
  ui::views::Checkbox* use_texture_ = nullptr;
  ui::views::Textfield* texture_path_ = nullptr;
  ui::views::Combobox* color_type_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_DEM_GRID_LOADER_DIALOG_H_

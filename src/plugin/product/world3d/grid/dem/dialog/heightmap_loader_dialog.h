// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEIGHTMAP_LOADER_DIALOG_H_
#define PLUGIN_WORLD3D_HEIGHTMAP_LOADER_DIALOG_H_

#include <string>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Checkbox;
class Combobox;
class Textfield;
}

namespace plugin {

// Heightmap (DEM raster) loader dialog. Layout from UiDesigner markup
// (world3d/heightmap_loader.ui.xml); this type only binds ids and processing.
class HeightmapLoaderDialog : public ui::views::View {
 public:
  explicit HeightmapLoaderDialog(content::PluginHost* host);

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

#endif  // PLUGIN_WORLD3D_HEIGHTMAP_LOADER_DIALOG_H_

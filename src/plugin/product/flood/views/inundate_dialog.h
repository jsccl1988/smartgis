// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_FLOOD_INUNDATE_DIALOG_H_
#define PLUGIN_FLOOD_INUNDATE_DIALOG_H_

#include <string>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Textfield;
}

namespace plugin {

// Collects DEM + seed + water params and runs flood.inundate.
class InundateDialog : public ui::views::View {
 public:
  explicit InundateDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_pick_dem();
  void on_pick_output();
  std::string build_json() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* dem_path_ = nullptr;
  ui::views::Textfield* output_path_ = nullptr;
  ui::views::Textfield* seed_x_ = nullptr;
  ui::views::Textfield* seed_y_ = nullptr;
  ui::views::Textfield* water_level_ = nullptr;
  ui::views::Textfield* water_depth_ = nullptr;
  ui::views::Textfield* frames_ = nullptr;
  ui::views::Textfield* frames_dir_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_FLOOD_INUNDATE_DIALOG_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORMSURGE_RUN_DIALOG_H_
#define PLUGIN_STORMSURGE_RUN_DIALOG_H_

#include <string>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Textfield;
}

namespace plugin {

// Collects DEM + coast + tide params and runs stormsurge.run.
class StormSurgeRunDialog : public ui::views::View {
 public:
  explicit StormSurgeRunDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_pick_dem();
  void on_pick_coast();
  void on_pick_output();
  std::string build_json() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* dem_path_ = nullptr;
  ui::views::Textfield* coast_path_ = nullptr;
  ui::views::Textfield* tide_path_ = nullptr;
  ui::views::Textfield* output_path_ = nullptr;
  ui::views::Textfield* seed_x_ = nullptr;
  ui::views::Textfield* seed_y_ = nullptr;
  ui::views::Textfield* tide_level_ = nullptr;
  ui::views::Textfield* frames_ = nullptr;
  ui::views::Textfield* frames_dir_ = nullptr;
  ui::views::Textfield* depth_path_ = nullptr;
  ui::views::Textfield* stats_path_ = nullptr;
  ui::views::Textfield* impact_path_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_STORMSURGE_RUN_DIALOG_H_

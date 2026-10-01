// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_TRAFFIC_SHORTEST_PATH_DIALOG_H_
#define PLUGIN_TRAFFIC_SHORTEST_PATH_DIALOG_H_

#include <string>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Textfield;
}

namespace plugin {

// Collects network path + endpoints and runs traffic.cost_path.
class ShortestPathDialog : public ui::views::View {
 public:
  explicit ShortestPathDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_pick_network();
  void on_pick_output();
  std::string build_json() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* network_path_ = nullptr;
  ui::views::Textfield* output_path_ = nullptr;
  ui::views::Textfield* start_x_ = nullptr;
  ui::views::Textfield* start_y_ = nullptr;
  ui::views::Textfield* end_x_ = nullptr;
  ui::views::Textfield* end_y_ = nullptr;
  ui::views::Textfield* weight_field_ = nullptr;
  ui::views::Textfield* frames_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_TRAFFIC_SHORTEST_PATH_DIALOG_H_

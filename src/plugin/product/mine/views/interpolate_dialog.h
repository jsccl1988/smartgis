// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MINE_INTERPOLATE_DIALOG_H_
#define PLUGIN_MINE_INTERPOLATE_DIALOG_H_

#include <string>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace ui::views {
class Textfield;
}

namespace plugin {

// Collects borehole CSV + stratum ids and runs mine.interpolate_stratum /
// mine.prism_volume.
class InterpolateDialog : public ui::views::View {
 public:
  explicit InterpolateDialog(content::PluginHost* host);

 private:
  void on_ok();
  void on_prism();
  void on_pick_csv();
  void on_pick_output();
  std::string build_interpolate_json() const;
  std::string build_prism_json() const;

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* csv_path_ = nullptr;
  ui::views::Textfield* output_path_ = nullptr;
  ui::views::Textfield* stratum_id_ = nullptr;
  ui::views::Textfield* top_stratum_ = nullptr;
  ui::views::Textfield* bottom_stratum_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_MINE_INTERPOLATE_DIALOG_H_

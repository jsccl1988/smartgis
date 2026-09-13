// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROJ_MAP_PRJ_DIALOG_H_
#define PLUGIN_PROJ_MAP_PRJ_DIALOG_H_

#include "ui/views/view.h"

namespace content {
class PluginHost;
}

namespace plugin {

class MapPrjGridPage;
class MapPrjXyPage;

// Tab host for projection transform (legacy CDlgMapPrj).
class MapPrjDialog : public ui::views::View {
 public:
  explicit MapPrjDialog(content::PluginHost* host);

 private:
  void sync_xy_scale();

  content::PluginHost* host_ = nullptr;
  MapPrjGridPage* grid_page_ = nullptr;
  MapPrjXyPage* xy_page_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_PROJ_MAP_PRJ_DIALOG_H_

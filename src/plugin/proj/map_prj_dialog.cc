// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/proj/map_prj_dialog.h"

#include <memory>

#include "content/public/plugin_host.h"
#include "plugin/proj/map_prj_grid_page.h"
#include "plugin/proj/map_prj_xy_page.h"
#include "ui/views/kernel/layout.h"
#include "ui/views/primitives/tab_strip.h"

namespace plugin {

MapPrjDialog::MapPrjDialog(content::PluginHost* host) : host_(host) {
  set_preferred_size({520, 400});
  set_layout_manager(std::make_unique<ui::views::FillLayout>());

  auto tabs = std::make_unique<ui::views::TabStrip>();
  auto* tabs_ptr = tabs.get();

  auto grid = std::make_unique<MapPrjGridPage>(host_);
  grid_page_ = grid.get();
  tabs_ptr->add_tab("Grid", std::move(grid));

  auto xy = std::make_unique<MapPrjXyPage>(host_);
  xy_page_ = xy.get();
  tabs_ptr->add_tab("XY", std::move(xy));

  sync_xy_scale();
  add_child(std::move(tabs));
}

void MapPrjDialog::sync_xy_scale() {
  if (grid_page_ && xy_page_) {
    xy_page_->set_scale_ruler(grid_page_->scale_ruler());
  }
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROJ_MAP_PRJ_GRID_PAGE_H_
#define PLUGIN_PROJ_MAP_PRJ_GRID_PAGE_H_

#include "ui/views/view.h"

namespace content {
class PluginHost;
}

namespace ui {
namespace views {
class Textfield;
}  // namespace views
}  // namespace ui

namespace plugin {

// Lat/lon grid projection tab page (legacy CDlgMapPrjDoGrid fields).
class MapPrjGridPage : public ui::views::View {
 public:
  explicit MapPrjGridPage(content::PluginHost* host);
  long scale_ruler() const;

 private:
  void on_apply();

  content::PluginHost* host_ = nullptr;
  ui::views::Textfield* dl_field_ = nullptr;
  ui::views::Textfield* db_field_ = nullptr;
  ui::views::Textfield* lmin_field_ = nullptr;
  ui::views::Textfield* bmin_field_ = nullptr;
  ui::views::Textfield* lmax_field_ = nullptr;
  ui::views::Textfield* bmax_field_ = nullptr;
  ui::views::Textfield* scale_field_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_PROJ_MAP_PRJ_GRID_PAGE_H_

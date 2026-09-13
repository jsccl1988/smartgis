// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROJ_MAP_PRJ_XY_PAGE_H_
#define PLUGIN_PROJ_MAP_PRJ_XY_PAGE_H_

#include "ui/views/view.h"

namespace content {
class PluginHost;
}

namespace ui {
namespace views {
class Label;
class Textfield;
}  // namespace views
}  // namespace ui

namespace plugin {

// Single-point lon/lat to projected XY (Gauss-Kruger) tab page.
class MapPrjXyPage : public ui::views::View {
 public:
  explicit MapPrjXyPage(content::PluginHost* host);
  void set_scale_ruler(long scale_ruler);

 private:
  void refresh_scale_label();
  void on_apply();

  content::PluginHost* host_ = nullptr;
  long scale_ruler_ = 1;
  ui::views::Label* scale_label_ = nullptr;
  ui::views::Textfield* l_field_ = nullptr;
  ui::views::Textfield* b_field_ = nullptr;
  ui::views::Textfield* x_field_ = nullptr;
  ui::views::Textfield* y_field_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_PROJ_MAP_PRJ_XY_PAGE_H_

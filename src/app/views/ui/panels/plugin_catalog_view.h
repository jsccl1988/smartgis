// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PANELS_PLUGIN_CATALOG_VIEW_H_
#define APP_VIEWS_UI_PANELS_PLUGIN_CATALOG_VIEW_H_

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace plugin {
class Registry;
}

namespace ui {
namespace views {
class Label;
class TableView;
}
}  // namespace ui

namespace app {

// Horizon plugin list: enable/disable plus hover tooltip with metadata.
class PluginCatalogView : public ui::views::View {
 public:
  PluginCatalogView(plugin::Registry* registry, content::PluginHost* host);

  static bool run_modal(HWND owner, plugin::Registry* registry,
                        content::PluginHost* host);

  void refresh();

 private:
  void on_enable();
  void on_disable();
  void on_hover_row(int row);
  std::string selected_id() const;
  std::string tooltip_for_row(int row) const;

  plugin::Registry* registry_ = nullptr;
  content::PluginHost* host_ = nullptr;
  ui::views::TableView* table_ = nullptr;
  ui::views::Label* tooltip_ = nullptr;
  ui::views::Label* error_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_PANELS_PLUGIN_CATALOG_VIEW_H_

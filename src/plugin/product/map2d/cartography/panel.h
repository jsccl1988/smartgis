// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_CARTOGRAPHY_PANEL_H_
#define PLUGIN_MAP2D_CARTOGRAPHY_PANEL_H_

namespace content {
class PluginHost;
}

namespace ui {
namespace views {
class MapFurniturePanel;
}
}  // namespace ui

namespace plugin {
namespace detail {

// Wire MapFurniturePanel callbacks through Map2dSink (no Browser*).
void wire_map_furniture_panel(ui::views::MapFurniturePanel* panel,
                              content::PluginHost* host);

// Contribute inspector dock map2d.furniture + View menu command.
bool contribute_map_furniture_panel(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_CARTOGRAPHY_PANEL_H_

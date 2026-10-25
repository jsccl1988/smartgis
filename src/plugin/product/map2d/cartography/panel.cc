// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/cartography/panel.h"

#include <memory>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/contribute.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "tool/command/command.h"
#include "ui/gis/shell/map_furniture_panel.h"

namespace plugin {
namespace detail {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";
constexpr const char* kDockId = "map2d.furniture";
constexpr const char* kCommandId = "map2d.furniture_panel";

}  // namespace

void wire_map_furniture_panel(ui::views::MapFurniturePanel* panel,
                              content::PluginHost* host) {
  if (!panel) {
    return;
  }
  Map2dSink* sink = map2d_sink(host);
  if (sink) {
    panel->set_scale_bar_checked(sink->furniture_scale_bar());
    panel->set_north_arrow_checked(sink->furniture_north_arrow());
    panel->set_border_checked(sink->furniture_border());
  }

  auto apply = [panel, sink]() {
    if (!sink) {
      return;
    }
    sink->set_furniture(panel->scale_bar_checked(), panel->north_arrow_checked(),
                        panel->border_checked());
    sink->invalidate();
  };
  panel->set_scale_bar_change([apply](bool) { apply(); });
  panel->set_north_arrow_change([apply](bool) { apply(); });
  panel->set_border_change([apply](bool) { apply(); });
}

bool contribute_map_furniture_panel(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains(kCommandId)) {
      return true;
    }
  }
  const content::DockContribution dock = {kDockId, "Map furniture", "right"};
  if (!host->contribute_dock(
          kPluginId, dock, [host](content::PluginHost*) {
            auto panel =
                make_plugin_view_unique<ui::views::MapFurniturePanel>();
            wire_map_furniture_panel(
                static_cast<ui::views::MapFurniturePanel*>(panel.get()), host);
            auto wrapped = wrap_with_present_surface(host, std::move(panel));
            if (ShellUiSink* ui = shell_ui(host)) {
              (void)ui->mount_inspector(kDockId, "Map furniture",
                                        std::move(wrapped));
            }
          })) {
    return false;
  }
  return contribute_command_aliases(
      host, kPluginId, {{kCommandId, "地图装饰"}}, "view",
      [host](const tool::CommandArgs&) {
        return host && host->open_dock(kDockId);
      });
}

}  // namespace detail
}  // namespace plugin

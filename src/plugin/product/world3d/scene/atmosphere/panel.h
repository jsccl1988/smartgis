// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_ATMOSPHERE_PANEL_H_
#define PLUGIN_WORLD3D_SCENE_ATMOSPHERE_PANEL_H_

namespace content {
class PluginHost;
}

namespace ui {
namespace views {
class AtmospherePanel;
}
}  // namespace ui

namespace plugin {
namespace detail {

// Wire AtmospherePanel callbacks through Scene3dSink (no Browser*).
void wire_atmosphere_panel(ui::views::AtmospherePanel* panel,
                           content::PluginHost* host);

// Contribute inspector dock world3d.atmosphere + View menu command.
bool contribute_atmosphere_panel(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_ATMOSPHERE_PANEL_H_

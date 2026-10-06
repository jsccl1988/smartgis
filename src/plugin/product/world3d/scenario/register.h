// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENARIO_REGISTER_H_
#define PLUGIN_WORLD3D_SCENARIO_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

bool register_world3d_showcase(content::PluginHost* host);

int scenario_world3d(HarnessShell& browser);
int scenario_world_preview(HarnessShell& browser);
int scenario_world_orthogrid(HarnessShell& browser);
int scenario_orthogrid3d(HarnessShell& browser);
int scenario_atmosphere_land(HarnessShell& browser);
int scenario_atmosphere_ocean(HarnessShell& browser);
int scenario_atmosphere_full(HarnessShell& browser);
int scenario_atmosphere_coast(HarnessShell& browser);
int scenario_atmosphere_legacy(HarnessShell& browser);
int scenario_atmosphere_globe(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENARIO_REGISTER_H_

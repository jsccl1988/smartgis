// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_REGISTER_H_
#define PLUGIN_MAP2D_SCENARIO_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

// GIS harness payloads owned by smartgis.map2d (edit / layers / navigate /
// present / milestones). Chrome HWND gates live in il.runtime/capability.
bool register_map2d_scenarios(content::PluginHost* host);

// Last map2d.scenario.* exit code after PluginHost::execute (0 = pass).
int scenario_last_exit_code();

int scenario_edit_m0(HarnessShell& browser);
int scenario_layers_m1(HarnessShell& browser);
int scenario_navigate(HarnessShell& browser);
int scenario_present(HarnessShell& browser);
int scenario_milestones(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_REGISTER_H_

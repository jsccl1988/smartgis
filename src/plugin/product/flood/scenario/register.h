// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_FLOOD_SCENARIO_REGISTER_H_
#define PLUGIN_FLOOD_SCENARIO_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

bool register_flood_scenario(content::PluginHost* host);
int scenario_flood(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_FLOOD_SCENARIO_REGISTER_H_

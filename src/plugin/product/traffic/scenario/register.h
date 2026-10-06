// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_TRAFFIC_SCENARIO_REGISTER_H_
#define PLUGIN_TRAFFIC_SCENARIO_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

bool register_traffic_scenario(content::PluginHost* host);
int scenario_traffic(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_TRAFFIC_SCENARIO_REGISTER_H_

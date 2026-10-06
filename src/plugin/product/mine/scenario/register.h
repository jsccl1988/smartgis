// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MINE_SCENARIO_REGISTER_H_
#define PLUGIN_MINE_SCENARIO_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

bool register_mine_scenario(content::PluginHost* host);
int scenario_mine(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_MINE_SCENARIO_REGISTER_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORMSURGE_SCENARIO_REGISTER_H_
#define PLUGIN_STORMSURGE_SCENARIO_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {

class HarnessShell;

bool register_stormsurge_scenario(content::PluginHost* host);
int scenario_stormsurge(HarnessShell& browser);

}  // namespace plugin

#endif  // PLUGIN_STORMSURGE_SCENARIO_REGISTER_H_

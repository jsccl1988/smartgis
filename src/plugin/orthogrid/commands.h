// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_COMMANDS_H_
#define PLUGIN_ORTHOGRID_COMMANDS_H_

namespace content {
class PluginHost;
}

namespace plugin {

// Registers boundary input/save/load commands and orth-grid processing.
bool register_orthogrid(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_ORTHOGRID_COMMANDS_H_

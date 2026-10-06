// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_COMMANDS_H_
#define PLUGIN_MAP2D_COMMANDS_H_

namespace content {
class PluginHost;
}

namespace plugin {

// China carto / align style / orthogrid mesh seed for --map2d-showcase=*.
bool register_map2d(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_COMMANDS_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_PRINT_REGISTER_H_
#define PLUGIN_MAP2D_PRINT_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// Print preview dialog + print.preview command, owned by smartgis.map2d.
bool contribute_print(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_MAP2D_PRINT_REGISTER_H_

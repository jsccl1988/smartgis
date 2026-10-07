// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_OPS_H_
#define PLUGIN_MAP2D_OPS_H_

namespace content {
class PluginHost;
}

namespace plugin {

// Commands + processing that talk to Map2dSink (open / frame / look / export).
bool register_map2d_sink_ops(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_OPS_H_

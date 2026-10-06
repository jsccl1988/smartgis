// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_COMMANDS_H_
#define PLUGIN_MAP2D_COMMANDS_H_

namespace content {
class PluginHost;
}

namespace plugin {

// China carto / align / orthogrid seed plus print.preview. Sole registrar
// for the merged map2d+print product pack (smartgis.map2d).
bool register_map2d(content::PluginHost* host);

// Seed command only (no print dialog, no scenarios). Showcase china/align
// must use this so register_map2d does not contribute print during Widget
// paint / seed.
bool register_map2d_seed(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_COMMANDS_H_

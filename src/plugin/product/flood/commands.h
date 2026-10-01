// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_FLOOD_COMMANDS_H_
#define PLUGIN_FLOOD_COMMANDS_H_

#include <functional>
#include <string>

namespace content {
class PluginHost;
}

namespace plugin {

// map2d / scene3d seam: flood mask frames (row-major Byte, 1=wet).
using FloodMaskWriter = std::function<bool(
    const unsigned char* mask, int width, int height, const double* geotransform,
    int frame_index, int frame_count, double water_level)>;

void set_flood_mask_writer(FloodMaskWriter writer);

bool register_flood(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_FLOOD_COMMANDS_H_

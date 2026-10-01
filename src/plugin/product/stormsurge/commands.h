// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORMSURGE_COMMANDS_H_
#define PLUGIN_STORMSURGE_COMMANDS_H_

#include <functional>
#include <string>

namespace content {
class PluginHost;
}

namespace plugin {

// map2d / scene3d seam: inundation mask frames (row-major Byte, 1=wet).
using StormSurgeMaskWriter = std::function<bool(
    const unsigned char* mask, int width, int height, const double* geotransform,
    int frame_index, int frame_count, double water_level)>;

// Water-surface triangle mesh (map CRS xyz + indices), peer of world3d surface.
using StormSurgeWaterMeshWriter = std::function<bool(
    const double* xyz, int point_count, const int* triangles, int triangle_count,
    int frame_index, int frame_count)>;

void set_stormsurge_mask_writer(StormSurgeMaskWriter writer);
void set_stormsurge_water_mesh_writer(StormSurgeWaterMeshWriter writer);

bool register_stormsurge(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_STORMSURGE_COMMANDS_H_

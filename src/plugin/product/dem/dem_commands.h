// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_DEM_DEM_COMMANDS_H_
#define PLUGIN_DEM_DEM_COMMANDS_H_

#include <functional>

namespace content {
class PluginHost;
}

namespace plugin {

// Views map commits the loaded TIN/grid. Unset writer keeps no_map_seam.
using DemSurfaceWriter = std::function<bool(
    const double* xyz, int point_count, const int* triangles,
    int triangle_count, const char* op)>;
void set_dem_surface_writer(DemSurfaceWriter writer);

bool register_dem(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_DEM_DEM_COMMANDS_H_

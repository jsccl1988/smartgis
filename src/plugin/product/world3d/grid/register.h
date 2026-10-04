// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_GRID_REGISTER_H_
#define PLUGIN_WORLD3D_GRID_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// Wires DEM loaders, 2D orthogrid, and 3D hex lattice under world3d/grid/.
bool register_world3d_grid(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_GRID_REGISTER_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_DEM_REGISTER_H_
#define PLUGIN_WORLD3D_DEM_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// DEM loaders, dialogs, and trimesh/heightmap processing.
bool register_world3d_dem(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_DEM_REGISTER_H_

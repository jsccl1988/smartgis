// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_HEXGRID_REGISTER_H_
#define PLUGIN_WORLD3D_HEXGRID_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// 3D hex lattice + VTK export under leftover orthogrid3d.* ids.
bool register_world3d_hexgrid(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_HEXGRID_REGISTER_H_

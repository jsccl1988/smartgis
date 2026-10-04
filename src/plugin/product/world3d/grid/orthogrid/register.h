// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_ORTHOGRID_REGISTER_H_
#define PLUGIN_WORLD3D_ORTHOGRID_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// 2D four-edge boundary digitize + Laplace/elliptic mesh. Contributes
// baogrid.* and orthogrid.* aliases from one handler set.
bool register_world3d_orthogrid(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_ORTHOGRID_REGISTER_H_

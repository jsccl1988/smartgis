// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_EARTH_REGISTER_H_
#define PLUGIN_WORLD3D_SCENE_EARTH_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// True-Earth browse: open_earth, fly_to, DEM/cloud/atmosphere, looks, globe fly.
bool register_world3d_earth(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_EARTH_REGISTER_H_

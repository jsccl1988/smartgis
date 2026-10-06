// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_REGISTER_H_
#define PLUGIN_WORLD3D_SCENE_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// Package scene façade: DEM / ortho / hex + True-Earth / model3d / pointcloud
// + Atmosphere dock.
bool register_world3d_scene(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_REGISTER_H_

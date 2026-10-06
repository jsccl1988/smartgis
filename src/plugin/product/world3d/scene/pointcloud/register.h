// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_POINTCLOUD_REGISTER_H_
#define PLUGIN_WORLD3D_SCENE_POINTCLOUD_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// Point-cloud load / PDAL processing contributed as world3d.* + model3d.*.
bool register_world3d_pointcloud(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_POINTCLOUD_REGISTER_H_

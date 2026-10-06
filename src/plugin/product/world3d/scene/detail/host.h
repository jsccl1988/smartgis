// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_DETAIL_HOST_H_
#define PLUGIN_WORLD3D_SCENE_DETAIL_HOST_H_

namespace content {
class PluginHost;
}

namespace plugin {
class Scene3dSink;
namespace detail {

Scene3dSink* world3d_scene_sink(content::PluginHost* host);
bool world3d_earth_ready(content::PluginHost* host);
bool fail_no_scene(const char* command);
bool fail_no_scene_processing(const char* command);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_DETAIL_HOST_H_

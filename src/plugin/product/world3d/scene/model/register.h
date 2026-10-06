// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_MODEL_REGISTER_H_
#define PLUGIN_WORLD3D_SCENE_MODEL_REGISTER_H_

namespace content {
class PluginHost;
}

namespace plugin {
namespace detail {

// Leftover model3d.* stand-ins (sphere / water / terrain / 2D-to-3D layer).
bool register_world3d_model(content::PluginHost* host);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_MODEL_REGISTER_H_

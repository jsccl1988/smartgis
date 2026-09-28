// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MODEL3D_COMMANDS_H_
#define PLUGIN_MODEL3D_COMMANDS_H_

#include <functional>
#include <string>

namespace content {
class PluginHost;
}

namespace plugin {

// Scene-device write seam for model3d. App shell installs when a real scene
// device exists; unset callbacks keep fail_no_scene / no_scene_device.
// Do not invent a stub OpenGL device inside this plugin.
struct Model3dSceneWriter {
  std::function<bool(const std::string& path)> add_pointcloud;
  std::function<bool()> add_sphere;
  std::function<bool()> add_water;
  std::function<bool()> add_terrain_grid;
  std::function<bool()> add_terrain_tin;
  std::function<bool()> layer_points_to_3d;
  std::function<bool()> layer_lines_to_3d;
  std::function<bool()> layer_polygons_to_3d;
  std::function<bool()> create_tin_from_active_layer;
};

void set_model3d_scene_writer(Model3dSceneWriter writer);

bool register_model3d(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_MODEL3D_COMMANDS_H_

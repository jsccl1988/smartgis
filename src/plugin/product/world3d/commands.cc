// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/register.h"

namespace plugin {

bool register_world3d(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  constexpr const char* kPluginId = "smartgis.world3d";
  // TIN / trimesh sample window used by plugin.world3d harness export_bmp.
  return detail::register_world3d_scene(host) &&
         host->contribute_export_frame(
             kPluginId, {"world3d_tin", 104.7, 34.7, 105.8, 35.8}) &&
         host->contribute_export_frame(
             kPluginId, {"world3d_trimesh", 104.7, 34.7, 105.8, 35.8});
}

}  // namespace plugin

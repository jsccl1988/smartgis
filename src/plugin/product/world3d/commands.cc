// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/register.h"
#include "tool/command/command.h"

namespace plugin {

// Scene + export-frame wiring for smartgis.world3d (DLL PLUGIN_PACK_REGISTER
// and ensure_world3d_pack). Pack prefixes (world3d / baogrid / orthogrid / …)
// register once in scenario/register.cc via ensure_world3d_pack — do not add
// register_command_pack here.
bool register_world3d(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  constexpr const char* kPluginId = "smartgis.world3d";
  if (tool::CommandCatalog* catalog = host->commands()) {
    // Scene registration owns create_hex_grid; treat as already wired.
    if (catalog->contains("orthogrid3d.create_hex_grid")) {
      return true;
    }
  }
  // TIN / trimesh sample window used by plugin.world3d harness export_bmp.
  return detail::register_world3d_scene(host) &&
         host->contribute_export_frame(
             kPluginId, {"world3d_tin", 104.7, 34.7, 105.8, 35.8}) &&
         host->contribute_export_frame(
             kPluginId, {"world3d_trimesh", 104.7, 34.7, 105.8, 35.8});
}

}  // namespace plugin

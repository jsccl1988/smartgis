// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/dem/dem_commands.h"

#include "content/public/plugin_host.h"
#include "plugin/dem/grid_loader_dialog.h"
#include "plugin/dem/tin_loader_dialog.h"
#include "plugin/widgets/about_dialog.h"
#include "tool/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.dem";

// Leftover tin_loader.cpp / grid_loader.cpp pull MFC via stdafx.h; stub until
// processing is moved to algorithm workers without MFC in this source_set.
bool stub_tin_from_xyz(content::PluginHost*, std::string_view) {
  return false;
}

bool stub_grid_from_heightmap(content::PluginHost*, std::string_view) {
  return false;
}

}  // namespace

bool register_dem(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  if (!host->contribute_command(
          kPluginId, "dem.load_tin", "离散点生成DEM", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("dem.tin_loader");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "dem.load_grid", "高度图生成DEM", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("dem.grid_loader");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "dem.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("dem.about");
          })) {
    return false;
  }

  if (!host->contribute_dialog(
          kPluginId, {"dem.tin_loader", "离散点生成DEM"},
          [host](content::PluginHost*) { TinLoaderDialog dialog(host); })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"dem.grid_loader", "高度图生成DEM"},
          [host](content::PluginHost*) { GridLoaderDialog dialog(host); })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"dem.about", "关于"},
          [](content::PluginHost*) {
            AboutDialog dialog("DEM Creater\nSmartGIS builtin plugin");
          })) {
    return false;
  }

  if (!host->contribute_processing(
          kPluginId, {"dem.tin_from_xyz", "TIN from XYZ"}, stub_tin_from_xyz)) {
    return false;
  }
  if (!host->contribute_processing(
          kPluginId, {"dem.grid_from_heightmap", "Heightmap to DEM grid"},
          stub_grid_from_heightmap)) {
    return false;
  }

  return true;
}

}  // namespace plugin

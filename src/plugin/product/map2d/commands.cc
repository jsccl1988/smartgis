// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/commands.h"

#include "content/public/plugin_host.h"
#include "plugin/product/map2d/seed/seed.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";

bool process_seed(content::PluginHost* host, std::string_view args_json) {
  return seed_map2d_from_json(host, args_json);
}

}  // namespace

bool register_map2d(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "map2d.seed", "加载二维样例", "tools",
          [host](const tool::CommandArgs& args) {
            return seed_map2d_from_json(host, args.payload);
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"map2d.seed", "Seed map2d showcase document"},
             process_seed) &&
         host->contribute_export_frame(
             kPluginId, {"map2d_china", 80.0, 20.0, 128.0, 53.5}) &&
         host->contribute_export_frame(
             kPluginId, {"map2d_orthogrid", 0.0, 0.0, 1.0, 1.0});
}

}  // namespace plugin

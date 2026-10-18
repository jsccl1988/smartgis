// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/gate.h"
#include "plugin/product/world3d/scene/atmosphere/panel.h"
#include "plugin/product/world3d/scene/register.h"
#include "plugin/runtime/host/capability/harness_command.h"
#include "plugin/runtime/host/capability/harness_shell.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/capability/product_gate.h"
#include "tool/command/command.h"

#include <string_view>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.world3d";

void prep_world3d_shell(HarnessShell* shell) {
  detail::bind_plugin_harness_shell(shell);
  detail::bind_atmosphere_harness_shell(shell);
}

bool world3d_gate_command(void* ctx) {
  auto* shell = static_cast<HarnessShell*>(ctx);
  return shell != nullptr && shell->plugin_host() != nullptr;
}

const ProductGateStep* world3d_gate_head() {
  static const ProductGateStep kHead[] = {
      {"contract", world3d_gate_contract},
      {"compute", world3d_gate_compute},
      {"command", world3d_gate_command},
  };
  return kHead;
}

// Showcase pictures live in the suite .py. The command id only runs the gate
// so argv dispatch and the script do not open the scene twice.
int run_showcase_gate(HarnessShell& browser) {
  const ProductGateResult result =
      run_product_gate("world3d", world3d_gate_head(), 3, &browser);
  return result.passed ? 0 : 1;
}

bool contribute_one(content::PluginHost* host, std::string_view command_id,
                    std::string_view title) {
  return contribute_harness_command(host, kPluginId, command_id, title,
                                    run_showcase_gate, prep_world3d_shell);
}

bool register_world3d_harness(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  return contribute_one(host, "world3d.scenario.world_preview",
                        "World3d world preview") &&
         contribute_one(host, "world3d.scenario.atmosphere.land",
                        "Atmosphere land showcase") &&
         contribute_one(host, "world3d.scenario.atmosphere.ocean",
                        "Atmosphere ocean showcase") &&
         contribute_one(host, "world3d.scenario.atmosphere.full",
                        "Atmosphere full showcase") &&
         contribute_one(host, "world3d.scenario.atmosphere.coast",
                        "Atmosphere coast showcase") &&
         contribute_one(host, "world3d.scenario.atmosphere.legacy",
                        "Atmosphere legacy showcase") &&
         contribute_one(host, "world3d.scenario.atmosphere.globe",
                        "Atmosphere globe showcase") &&
         contribute_one(host, "world3d.scenario.atmosphere.china_stack",
                        "China DEM ocean fog sky cloud stack gate");
}

// Single ensure entry for every world3d-owned pack prefix.
bool ensure_world3d_pack(content::PluginHost* host) {
  return register_world3d(host) && register_world3d_harness(host);
}

struct World3dPackOnce {
  World3dPackOnce() {
    register_command_pack("world3d", ensure_world3d_pack);
    register_command_pack("baogrid", ensure_world3d_pack);
    register_command_pack("orthogrid", ensure_world3d_pack);
    // Boundary match: orthogrid vs orthogrid3d; keep an explicit prefix.
    register_command_pack("orthogrid3d", ensure_world3d_pack);
    register_command_pack("model3d", ensure_world3d_pack);
    register_command_pack("atmosphere", ensure_world3d_pack);
  }
} k_world3d_pack_once;

}  // namespace

// Scene + export-frame wiring for smartgis.world3d (DLL PLUGIN_PACK_REGISTER
// and ensure_world3d_pack). Pack prefixes register once above.
bool register_world3d(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  constexpr const char* kPluginId = "smartgis.world3d";
  if (tool::CommandCatalog* catalog = host->commands()) {
    // Scene registration owns create_hex_grid; treat as already wired.
    // Scene commands may already exist (DLL pack or a prior ensure). That used
    // to return before contribute_atmosphere_panel, so the dock never landed.
    if (catalog->contains("orthogrid3d.create_hex_grid")) {
      return detail::contribute_atmosphere_panel(host);
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

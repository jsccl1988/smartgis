// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/register.h"

#include "content/browser/camera/orbit_frame.h"
#include "plugin/product/world3d/scenario/atmosphere/mode.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"
#include "plugin/product/world3d/scenario/atmosphere/seed/mode_seed.h"
#include "plugin/product/world3d/scenario/atmosphere/present/present_run.h"
#include "plugin/product/world3d/scenario/atmosphere/session/session_finish.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/product/world3d/scenario/interact.h"
#include "plugin/product/world3d/scenario/product/orthogrid.h"
#include "plugin/product/world3d/scenario/product/orthogrid3d.h"
#include "plugin/product/world3d/scenario/product/world3d.h"
#include "plugin/product/world3d/scenario/product/world_preview.h"
#include "plugin/runtime/host/capability/scenario_command.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "tool/command/command.h"

#include <cstdio>
#include <string_view>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.world3d";

void prep_world3d_shell(HarnessShell* shell) {
  detail::bind_plugin_scenario_shell(shell);
  detail::bind_atmosphere_scenario_shell(shell);
}

bool contribute_one(content::PluginHost* host, std::string_view command_id,
                    std::string_view title, HarnessScenarioFn fn) {
  return contribute_scenario_command(host, kPluginId, command_id, title, fn,
                                     prep_world3d_shell);
}

int run_atmosphere(HarnessShell& browser, AtmosphereShowcaseMode mode) {
  const char* name = atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  detail::atmosphere_mark(name);
  detail::atmosphere_mark("tab3d");
  detail::atmosphere_mark("pumped");

  detail::AtmosphereDeviceSession session;
  if (const int rc = detail::prepare_atmosphere_device_session(browser, &session)) {
    detail::finish_atmosphere_device_session(browser, &session, true);
    return rc;
  }
  content::Scene3dPresenter* cam = browser.scene3d();
  content::OrbitFrame* orbit = browser.orbit_frame();
  if (!cam || !orbit) {
    detail::finish_atmosphere_device_session(browser, &session, true);
    return 50;
  }
  detail::AtmosphereModeSeed seed;
  if (const int rc =
          detail::seed_atmosphere_mode(browser, mode, cam, orbit, &seed)) {
    detail::finish_atmosphere_device_session(browser, &session, true);
    return rc;
  }
  if (const int rc = detail::verify_atmosphere_mode_flags(mode, cam)) {
    detail::finish_atmosphere_device_session(browser, &session, true);
    return rc;
  }
  return detail::run_atmosphere_present(browser, mode, name, &session, cam,
                                        orbit, seed);
}

}  // namespace

int scenario_world3d(HarnessShell& browser) {
  return detail::run_world3d_scene3d(browser);
}
int scenario_world_preview(HarnessShell& browser) {
  return detail::run_world_preview(browser);
}
int scenario_world_orthogrid(HarnessShell& browser) {
  return detail::run_orthogrid(browser);
}
int scenario_orthogrid3d(HarnessShell& browser) {
  return detail::run_orthogrid3d(browser);
}
int scenario_atmosphere_land(HarnessShell& browser) {
  return run_atmosphere(browser, AtmosphereShowcaseMode::kLand);
}
int scenario_atmosphere_ocean(HarnessShell& browser) {
  return run_atmosphere(browser, AtmosphereShowcaseMode::kOcean);
}
int scenario_atmosphere_full(HarnessShell& browser) {
  return run_atmosphere(browser, AtmosphereShowcaseMode::kFull);
}
int scenario_atmosphere_coast(HarnessShell& browser) {
  return run_atmosphere(browser, AtmosphereShowcaseMode::kCoast);
}
int scenario_atmosphere_legacy(HarnessShell& browser) {
  return run_atmosphere(browser, AtmosphereShowcaseMode::kLegacy);
}
int scenario_atmosphere_globe(HarnessShell& browser) {
  return run_atmosphere(browser, AtmosphereShowcaseMode::kGlobe);
}

bool register_world3d_scenario(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  // Per-id contribute_one is idempotent; do not bail after only world3d.*.
  return contribute_one(host, "world3d.scenario.showcase",
                        "World3d Scene3D showcase", scenario_world3d) &&
         contribute_one(host, "world3d.scenario.world_preview",
                        "World3d world_preview showcase",
                        scenario_world_preview) &&
         contribute_one(host, "orthogrid.scenario.showcase",
                        "Orthogrid Map2d showcase", scenario_world_orthogrid) &&
         contribute_one(host, "orthogrid3d.scenario.showcase",
                        "Orthogrid3d Scene3D showcase", scenario_orthogrid3d) &&
         contribute_one(host, "world3d.scenario.atmosphere.land",
                        "Atmosphere land showcase", scenario_atmosphere_land) &&
         contribute_one(host, "world3d.scenario.atmosphere.ocean",
                        "Atmosphere ocean showcase",
                        scenario_atmosphere_ocean) &&
         contribute_one(host, "world3d.scenario.atmosphere.full",
                        "Atmosphere full showcase", scenario_atmosphere_full) &&
         contribute_one(host, "world3d.scenario.atmosphere.coast",
                        "Atmosphere coast showcase",
                        scenario_atmosphere_coast) &&
         contribute_one(host, "world3d.scenario.atmosphere.legacy",
                        "Atmosphere legacy showcase",
                        scenario_atmosphere_legacy) &&
         contribute_one(host, "world3d.scenario.atmosphere.globe",
                        "Atmosphere globe showcase", scenario_atmosphere_globe);
}

namespace {

// Single ensure entry for every world3d-owned pack prefix. Chains scene /
// command registration (register_world3d) then harness scenario contributions
// (register_world3d_scenario). Do not also call register_command_pack from
// commands.cc -- overlapping prefixes would append a second ensure and fight.
bool ensure_world3d_pack(content::PluginHost* host) {
  // DLL PLUGIN_PACK_REGISTER / prior ensure may already have wired scenes;
  // ignore false so scenario contribute still runs.
  (void)register_world3d(host);
  return register_world3d_scenario(host);
}

struct World3dHarnessOnce {
  World3dHarnessOnce() {
    // Static init before any ensure_for_command: interact ops must be present
    // when packs are later ensured.
    register_world3d_interact_ops();
    // One ensure fn per prefix -- covers scene + scenario together.
    register_command_pack("world3d", ensure_world3d_pack);
    register_command_pack("baogrid", ensure_world3d_pack);
    register_command_pack("orthogrid", ensure_world3d_pack);
    // Boundary match: orthogrid vs orthogrid3d; keep an explicit prefix.
    register_command_pack("orthogrid3d", ensure_world3d_pack);
    register_command_pack("model3d", ensure_world3d_pack);
    register_command_pack("atmosphere", ensure_world3d_pack);
  }
} k_world3d_harness_once;

}  // namespace

}  // namespace plugin

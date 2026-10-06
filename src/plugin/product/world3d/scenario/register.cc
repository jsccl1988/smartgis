// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/register.h"

#include "app/views/app/cmdline/views_launch_options.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scenario/atmosphere/session/device_session.h"
#include "plugin/product/world3d/scenario/atmosphere/seed/mode_seed.h"
#include "plugin/product/world3d/scenario/atmosphere/present/present_run.h"
#include "plugin/product/world3d/scenario/atmosphere/common/progress.h"
#include "plugin/product/world3d/scenario/atmosphere/session/session_finish.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "plugin/product/world3d/scenario/interact.h"
#include "plugin/product/world3d/scenario/product/orthogrid.h"
#include "plugin/product/world3d/scenario/product/orthogrid3d.h"
#include "plugin/product/world3d/scenario/product/world3d.h"
#include "plugin/product/world3d/scenario/product/world_preview.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/capability/shell.h"
#include "tool/command/command.h"

#include <cstdio>
#include <string_view>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.world3d";

bool run_bound(content::PluginHost* host, int (*fn)(HarnessShell&)) {
  HarnessShell* shell = harness_shell(host);
  if (!shell || !fn) {
    set_harness_scenario_exit(1);
    return false;
  }
  detail::bind_plugin_showcase_shell(shell);
  detail::bind_atmosphere_showcase_shell(shell);
  set_harness_scenario_exit(fn(*shell));
  return harness_scenario_exit() == 0;
}

bool contribute_one(content::PluginHost* host, std::string_view command_id,
                    std::string_view title, int (*fn)(HarnessShell&)) {
  return host->contribute_command(
      kPluginId, command_id, title, "tools",
      [host, fn](const tool::CommandArgs&) { return run_bound(host, fn); });
}

int run_atmosphere(HarnessShell& browser, app::AtmosphereShowcaseMode mode) {
  const char* name = app::atmosphere_showcase_name(mode);
  std::fprintf(stderr, "atmosphere-showcase mode=%s\n", name);
  detail::atmosphere_showcase_mark(name);
  detail::atmosphere_showcase_mark("tab3d");
  detail::atmosphere_showcase_mark("pumped");

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
  return run_atmosphere(browser, app::AtmosphereShowcaseMode::kLand);
}
int scenario_atmosphere_ocean(HarnessShell& browser) {
  return run_atmosphere(browser, app::AtmosphereShowcaseMode::kOcean);
}
int scenario_atmosphere_full(HarnessShell& browser) {
  return run_atmosphere(browser, app::AtmosphereShowcaseMode::kFull);
}
int scenario_atmosphere_coast(HarnessShell& browser) {
  return run_atmosphere(browser, app::AtmosphereShowcaseMode::kCoast);
}
int scenario_atmosphere_legacy(HarnessShell& browser) {
  return run_atmosphere(browser, app::AtmosphereShowcaseMode::kLegacy);
}
int scenario_atmosphere_globe(HarnessShell& browser) {
  return run_atmosphere(browser, app::AtmosphereShowcaseMode::kGlobe);
}

bool register_world3d_showcase(content::PluginHost* host) {
  register_world3d_interact_verbs();
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("world3d.scenario.showcase")) {
      return true;
    }
  }
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
struct World3dInteractOnce {
  World3dInteractOnce() { register_world3d_interact_verbs(); }
} k_world3d_interact_once;
}  // namespace

}  // namespace plugin

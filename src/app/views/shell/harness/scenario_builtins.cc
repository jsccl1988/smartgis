// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/scenario_registry.h"

#include <mutex>

#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/atmosphere/atmosphere_showcase.h"
#include "app/views/shell/harness/showcase/browse/browse_showcase.h"
#include "app/views/shell/harness/showcase/input/input_showcase.h"
#include "app/views/shell/harness/showcase/map2d/map2d_showcase.h"
#include "app/views/shell/harness/showcase/plugin/plugin_showcase.h"
#include "app/views/shell/harness/showcase/ui/ui_showcase.h"

namespace app {
namespace {

int run_ui_shell(Browser& browser) {
  return run_ui_showcase(browser, UiShowcaseMode::kShell);
}

int run_ui_data(Browser& browser) {
  return run_ui_showcase(browser, UiShowcaseMode::kData);
}

int run_ui_scene(Browser& browser) {
  return run_ui_showcase(browser, UiShowcaseMode::kScene);
}

int run_ui_catalog(Browser& browser) {
  return run_ui_showcase(browser, UiShowcaseMode::kCatalog);
}

int run_ui_interact(Browser& browser) {
  return run_ui_showcase(browser, UiShowcaseMode::kInteract);
}

int run_map2d_china(Browser& browser) {
  return run_map2d_showcase(browser, Map2dShowcaseMode::kChina);
}

int run_map2d_align(Browser& browser) {
  return run_map2d_showcase(browser, Map2dShowcaseMode::kAlign);
}

int run_map2d_orthogrid(Browser& browser) {
  return run_map2d_showcase(browser, Map2dShowcaseMode::kOrthogrid);
}

int run_plugin_world3d(Browser& browser) {
  return run_plugin_showcase(browser, "world3d");
}

int run_plugin_world_preview(Browser& browser) {
  return run_plugin_showcase(browser, "world_preview");
}

int run_plugin_print(Browser& browser) {
  return run_plugin_showcase(browser, "print");
}

int run_plugin_orthogrid(Browser& browser) {
  return run_plugin_showcase(browser, "orthogrid");
}

int run_plugin_orthogrid3d(Browser& browser) {
  return run_plugin_showcase(browser, "orthogrid3d");
}

int run_plugin_traffic(Browser& browser) {
  return run_plugin_showcase(browser, "traffic");
}

int run_plugin_flood(Browser& browser) {
  return run_plugin_showcase(browser, "flood");
}

int run_plugin_stormsurge(Browser& browser) {
  return run_plugin_showcase(browser, "stormsurge");
}

int run_plugin_mine(Browser& browser) {
  return run_plugin_showcase(browser, "mine");
}

int run_plugin_geochem(Browser& browser) {
  return run_plugin_showcase(browser, "geochem");
}

int run_plugin_report(Browser& browser) {
  return run_plugin_showcase(browser, "report");
}

int run_atmosphere_land(Browser& browser) {
  return run_atmosphere_showcase(browser, AtmosphereShowcaseMode::kLand);
}

int run_atmosphere_ocean(Browser& browser) {
  return run_atmosphere_showcase(browser, AtmosphereShowcaseMode::kOcean);
}

int run_atmosphere_full(Browser& browser) {
  return run_atmosphere_showcase(browser, AtmosphereShowcaseMode::kFull);
}

int run_atmosphere_coast(Browser& browser) {
  return run_atmosphere_showcase(browser, AtmosphereShowcaseMode::kCoast);
}

int run_atmosphere_legacy(Browser& browser) {
  return run_atmosphere_showcase(browser, AtmosphereShowcaseMode::kLegacy);
}

int run_atmosphere_globe(Browser& browser) {
  return run_atmosphere_showcase(browser, AtmosphereShowcaseMode::kGlobe);
}

}  // namespace

void ensure_builtin_scenarios() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Ids align with testing/tools/harness/<family>/<suite_id>/suite.json where a suite exists.
    register_scenario(Scenario{
        "browse",
        ScenarioKind::kShowcase,
        detail::kSelfTestMarkLeaf,
        &run_browse_showcase,
    });
    register_scenario(Scenario{
        "browse.3d",
        ScenarioKind::kShowcase,
        detail::kBrowse3dMarkLeaf,
        &run_browse_showcase,
    });
    register_scenario(Scenario{
        "self_test",
        ScenarioKind::kSelfTest,
        detail::kSelfTestMarkLeaf,
        &run_views_self_test,
    });
    register_scenario(Scenario{
        "console",
        ScenarioKind::kSelfTest,
        detail::kSelfTestMarkLeaf,
        &run_views_console_self_test,
    });
    register_scenario(Scenario{
        "input",
        ScenarioKind::kShowcase,
        detail::kInputShowcaseMarkLeaf,
        &run_input_showcase,
    });
    register_scenario(Scenario{
        "ui.shell",
        ScenarioKind::kShowcase,
        detail::kUiShowcaseMarkLeaf,
        &run_ui_shell,
    });
    register_scenario(Scenario{
        "ui.data",
        ScenarioKind::kShowcase,
        detail::kUiShowcaseMarkLeaf,
        &run_ui_data,
    });
    register_scenario(Scenario{
        "ui.scene",
        ScenarioKind::kShowcase,
        detail::kUiShowcaseMarkLeaf,
        &run_ui_scene,
    });
    register_scenario(Scenario{
        "ui.catalog",
        ScenarioKind::kShowcase,
        detail::kUiShowcaseMarkLeaf,
        &run_ui_catalog,
    });
    register_scenario(Scenario{
        "ui.interact",
        ScenarioKind::kShowcase,
        detail::kUiShowcaseMarkLeaf,
        &run_ui_interact,
    });
    register_scenario(Scenario{
        "map2d.china",
        ScenarioKind::kShowcase,
        detail::kMap2dShowcaseMarkLeaf,
        &run_map2d_china,
    });
    register_scenario(Scenario{
        "map2d.align",
        ScenarioKind::kShowcase,
        detail::kMap2dShowcaseMarkLeaf,
        &run_map2d_align,
    });
    register_scenario(Scenario{
        "map2d.orthogrid",
        ScenarioKind::kShowcase,
        detail::kMap2dShowcaseMarkLeaf,
        &run_map2d_orthogrid,
    });
    register_scenario(Scenario{
        "plugin.world3d",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_world3d,
    });
    register_scenario(Scenario{
        "plugin.world_preview",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_world_preview,
    });
    register_scenario(Scenario{
        "plugin.print",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_print,
    });
    register_scenario(Scenario{
        "plugin.orthogrid",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_orthogrid,
    });
    register_scenario(Scenario{
        "plugin.orthogrid3d",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_orthogrid3d,
    });
    register_scenario(Scenario{
        "plugin.traffic",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_traffic,
    });
    register_scenario(Scenario{
        "plugin.flood",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_flood,
    });
    register_scenario(Scenario{
        "plugin.stormsurge",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_stormsurge,
    });
    register_scenario(Scenario{
        "plugin.mine",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_mine,
    });
    register_scenario(Scenario{
        "plugin.geochem",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_geochem,
    });
    register_scenario(Scenario{
        "plugin.report",
        ScenarioKind::kShowcase,
        detail::kPluginShowcaseMarkLeaf,
        &run_plugin_report,
    });
    register_scenario(Scenario{
        "atmosphere.land",
        ScenarioKind::kShowcase,
        detail::kAtmosphereShowcaseMarkLeaf,
        &run_atmosphere_land,
    });
    register_scenario(Scenario{
        "atmosphere.ocean",
        ScenarioKind::kShowcase,
        detail::kAtmosphereShowcaseMarkLeaf,
        &run_atmosphere_ocean,
    });
    register_scenario(Scenario{
        "atmosphere.full",
        ScenarioKind::kShowcase,
        detail::kAtmosphereShowcaseMarkLeaf,
        &run_atmosphere_full,
    });
    register_scenario(Scenario{
        "atmosphere.coast",
        ScenarioKind::kShowcase,
        detail::kAtmosphereShowcaseMarkLeaf,
        &run_atmosphere_coast,
    });
    register_scenario(Scenario{
        "atmosphere.legacy",
        ScenarioKind::kShowcase,
        detail::kAtmosphereShowcaseMarkLeaf,
        &run_atmosphere_legacy,
    });
    register_scenario(Scenario{
        "atmosphere.globe",
        ScenarioKind::kShowcase,
        detail::kAtmosphereShowcaseMarkLeaf,
        &run_atmosphere_globe,
    });
  });
}

}  // namespace app

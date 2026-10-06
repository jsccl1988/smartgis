// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/scenario_registry.h"

#include <mutex>

#include "app/views/app/cmdline/views_launch_options.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/self_test/self_test.h"
#include "app/views/harness/showcase/atmosphere/atmosphere_showcase.h"
#include "app/views/harness/showcase/browse/browse_showcase.h"
#include "app/views/harness/showcase/input/input_showcase.h"
#include "app/views/harness/showcase/map2d/map2d_showcase.h"
#include "app/views/harness/showcase/plugin/plugin_showcase.h"
#include "app/views/harness/showcase/ui/ui_showcase.h"

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

constexpr LaunchPolicy k_gdi_skip_overlay{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, true};
constexpr LaunchPolicy k_gdi_skip_no_overlay{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, false};
constexpr LaunchPolicy k_gdi_product_horizon{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, true};
constexpr LaunchPolicy k_gdi_self_test{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, false};
constexpr LaunchPolicy k_plugin_scene3d{
    Scene3dStartup::kFlyCube, Map2dStartup::kPluginScene3d, true, true};
constexpr LaunchPolicy k_browse_3d{
    Scene3dStartup::kFlyCube, Map2dStartup::kFlyCube2d, true, false};
constexpr LaunchPolicy k_ui_scene{
    Scene3dStartup::kFlyCube, Map2dStartup::kFlyCube2d, false, false};
constexpr LaunchPolicy k_ui_interact{
    Scene3dStartup::kFlyCube, Map2dStartup::kInteract, false, false};

void add_showcase(const char* id, const wchar_t* mark, int (*run)(Browser&),
                  const LaunchPolicy& policy) {
  register_scenario(Scenario{id, ScenarioKind::kShowcase, mark, run, policy});
}

void add_self_test(const char* id, const wchar_t* mark, int (*run)(Browser&),
                   const LaunchPolicy& policy) {
  register_scenario(Scenario{id, ScenarioKind::kSelfTest, mark, run, policy});
}

}  // namespace

void ensure_builtin_scenarios() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Ids align with testing/tools/harness/<family>/<suite_id>/suite.json.
    add_showcase("browse", detail::kSelfTestMarkLeaf, &run_browse_showcase,
                 k_gdi_skip_overlay);
    add_showcase("browse.3d", detail::kBrowse3dMarkLeaf, &run_browse_showcase,
                 k_browse_3d);
    add_self_test("self_test", detail::kSelfTestMarkLeaf, &run_views_self_test,
                  k_gdi_self_test);
    add_self_test("console", detail::kSelfTestMarkLeaf,
                  &run_views_console_self_test, k_gdi_self_test);
    add_showcase("input", detail::kInputShowcaseMarkLeaf, &run_input_showcase,
                 k_gdi_skip_no_overlay);
    add_showcase("ui.shell", detail::kUiShowcaseMarkLeaf, &run_ui_shell,
                 k_gdi_product_horizon);
    add_showcase("ui.data", detail::kUiShowcaseMarkLeaf, &run_ui_data,
                 k_gdi_product_horizon);
    add_showcase("ui.scene", detail::kUiShowcaseMarkLeaf, &run_ui_scene,
                 k_ui_scene);
    add_showcase("ui.catalog", detail::kUiShowcaseMarkLeaf, &run_ui_catalog,
                 k_gdi_product_horizon);
    add_showcase("ui.interact", detail::kUiShowcaseMarkLeaf, &run_ui_interact,
                 k_ui_interact);
    add_showcase("map2d.china", detail::kMap2dShowcaseMarkLeaf, &run_map2d_china,
                 k_gdi_skip_overlay);
    add_showcase("map2d.align", detail::kMap2dShowcaseMarkLeaf, &run_map2d_align,
                 k_gdi_skip_overlay);
    add_showcase("map2d.orthogrid", detail::kMap2dShowcaseMarkLeaf,
                 &run_map2d_orthogrid, k_gdi_skip_overlay);
    add_showcase("plugin.world3d", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_world3d, k_plugin_scene3d);
    add_showcase("plugin.world_preview", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_world_preview, k_plugin_scene3d);
    add_showcase("plugin.print", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_print, k_gdi_skip_overlay);
    add_showcase("plugin.orthogrid", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_orthogrid, k_gdi_skip_overlay);
    add_showcase("plugin.orthogrid3d", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_orthogrid3d, k_plugin_scene3d);
    add_showcase("plugin.traffic", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_traffic, k_gdi_skip_overlay);
    add_showcase("plugin.flood", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_flood, k_gdi_skip_overlay);
    add_showcase("plugin.stormsurge", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_stormsurge, k_plugin_scene3d);
    add_showcase("plugin.mine", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_mine, k_plugin_scene3d);
    add_showcase("plugin.geochem", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_geochem, k_gdi_skip_overlay);
    add_showcase("plugin.report", detail::kPluginShowcaseMarkLeaf,
                 &run_plugin_report, k_gdi_skip_overlay);
    add_showcase("atmosphere.land", detail::kAtmosphereShowcaseMarkLeaf,
                 &run_atmosphere_land, k_gdi_skip_no_overlay);
    add_showcase("atmosphere.ocean", detail::kAtmosphereShowcaseMarkLeaf,
                 &run_atmosphere_ocean, k_gdi_skip_no_overlay);
    add_showcase("atmosphere.full", detail::kAtmosphereShowcaseMarkLeaf,
                 &run_atmosphere_full, k_gdi_skip_no_overlay);
    add_showcase("atmosphere.coast", detail::kAtmosphereShowcaseMarkLeaf,
                 &run_atmosphere_coast, k_gdi_skip_no_overlay);
    add_showcase("atmosphere.legacy", detail::kAtmosphereShowcaseMarkLeaf,
                 &run_atmosphere_legacy, k_gdi_skip_no_overlay);
    add_showcase("atmosphere.globe", detail::kAtmosphereShowcaseMarkLeaf,
                 &run_atmosphere_globe, k_gdi_skip_no_overlay);
  });
}

}  // namespace app

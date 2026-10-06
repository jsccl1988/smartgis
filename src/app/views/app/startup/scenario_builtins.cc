// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/scenario.h"

#include <mutex>

#include "app/views/browser/plugin/report_suite.h"
#include "app/views/il.runtime/backend/mark.h"

namespace app {
namespace {

constexpr LaunchPolicy k_gdi_skip_overlay{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, true};
constexpr LaunchPolicy k_gdi_skip_no_overlay{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, false};
constexpr LaunchPolicy k_gdi_product_horizon{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, true};
constexpr LaunchPolicy k_gdi_harness{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, false};
constexpr LaunchPolicy k_plugin_scene3d{
    Scene3dStartup::kFlyCube, Map2dStartup::kPluginScene3d, true, true};
constexpr LaunchPolicy k_browse_3d{
    Scene3dStartup::kFlyCube, Map2dStartup::kFlyCube2d, true, false};
constexpr LaunchPolicy k_ui_scene{
    Scene3dStartup::kFlyCube, Map2dStartup::kFlyCube2d, false, false};
constexpr LaunchPolicy k_ui_interact{
    Scene3dStartup::kFlyCube, Map2dStartup::kInteract, false, false};

void add_row(const Scenario& row) { register_scenario(row); }

}  // namespace

void ensure_builtin_scenarios() {
  static std::once_flag once;
  std::call_once(once, [] {
    add_row(Scenario{"browse", ScenarioKind::kShowcase, detail::kHarnessMarkLeaf,
                     "browse", nullptr, nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"browse.3d", ScenarioKind::kShowcase,
                     detail::kBrowse3dMarkLeaf, "browse.3d", nullptr, nullptr,
                     k_browse_3d});
    add_row(Scenario{"harness", ScenarioKind::kHarness, detail::kHarnessMarkLeaf,
                     "harness", nullptr, nullptr, k_gdi_harness});
    add_row(Scenario{"self_test", ScenarioKind::kHarness,
                     detail::kHarnessMarkLeaf, "harness", nullptr, nullptr,
                     k_gdi_harness});
    add_row(Scenario{"console", ScenarioKind::kHarness, detail::kHarnessMarkLeaf,
                     "console", nullptr, nullptr, k_gdi_harness});
    add_row(Scenario{"input", ScenarioKind::kShowcase,
                     detail::kInputShowcaseMarkLeaf, "input", nullptr, nullptr,
                     k_gdi_skip_no_overlay});
    add_row(Scenario{"ui.shell", ScenarioKind::kShowcase,
                     detail::kUiShowcaseMarkLeaf, "ui.shell", nullptr, nullptr,
                     k_gdi_product_horizon});
    add_row(Scenario{"ui.data", ScenarioKind::kShowcase,
                     detail::kUiShowcaseMarkLeaf, "ui.data", nullptr, nullptr,
                     k_gdi_product_horizon});
    add_row(Scenario{"ui.scene", ScenarioKind::kShowcase,
                     detail::kUiShowcaseMarkLeaf, "ui.scene", nullptr, nullptr,
                     k_ui_scene});
    add_row(Scenario{"ui.catalog", ScenarioKind::kShowcase,
                     detail::kUiShowcaseMarkLeaf, "ui.catalog", nullptr, nullptr,
                     k_gdi_product_horizon});
    add_row(Scenario{"ui.interact", ScenarioKind::kShowcase,
                     detail::kUiShowcaseMarkLeaf, "ui.interact.host", nullptr,
                     nullptr, k_ui_interact});
    add_row(Scenario{"browser.map2d.china", ScenarioKind::kShowcase,
                     detail::kMap2dShowcaseMarkLeaf, "browser.map2d.china",
                     "map2d.scenario.china", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"browser.map2d.align", ScenarioKind::kShowcase,
                     detail::kMap2dShowcaseMarkLeaf, "browser.map2d.align",
                     "map2d.scenario.align", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"browser.map2d.orthogrid", ScenarioKind::kShowcase,
                     detail::kMap2dShowcaseMarkLeaf, "browser.map2d.orthogrid",
                     "map2d.scenario.orthogrid", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.world3d", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.world3d",
                     "world3d.scenario.showcase", nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.world3d.preview", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.world3d.preview",
                     "world3d.scenario.world_preview", nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.print", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.print",
                     "map2d.scenario.print", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.orthogrid", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf,
                     "plugin.orthogrid", "orthogrid.scenario.showcase",
                     nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.orthogrid3d", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf,
                     "plugin.orthogrid3d",
                     "orthogrid3d.scenario.showcase", nullptr,
                     k_plugin_scene3d});
    add_row(Scenario{"plugin.traffic", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.traffic",
                     "traffic.scenario.showcase", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.flood", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.flood",
                     "flood.scenario.showcase", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.stormsurge", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.stormsurge",
                     "stormsurge.scenario.showcase", nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.mine", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.mine",
                     "mine.scenario.showcase", nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.geochem", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, "plugin.geochem",
                     "geochem.scenario.showcase", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.report", ScenarioKind::kShowcase,
                     detail::kPluginShowcaseMarkLeaf, nullptr, nullptr,
                     &run_report_suite, k_gdi_skip_overlay});
    add_row(Scenario{"browser.world3d.land", ScenarioKind::kShowcase,
                     detail::kAtmosphereShowcaseMarkLeaf, "browser.world3d.land",
                     "world3d.scenario.atmosphere.land", nullptr,
                     k_gdi_skip_no_overlay});
    add_row(Scenario{"browser.world3d.ocean", ScenarioKind::kShowcase,
                     detail::kAtmosphereShowcaseMarkLeaf,
                     "browser.world3d.ocean",
                     "world3d.scenario.atmosphere.ocean", nullptr,
                     k_gdi_skip_no_overlay});
    add_row(Scenario{"browser.world3d.full", ScenarioKind::kShowcase,
                     detail::kAtmosphereShowcaseMarkLeaf, "browser.world3d.full",
                     "world3d.scenario.atmosphere.full", nullptr,
                     k_gdi_skip_no_overlay});
    add_row(Scenario{"browser.world3d.coast", ScenarioKind::kShowcase,
                     detail::kAtmosphereShowcaseMarkLeaf,
                     "browser.world3d.coast",
                     "world3d.scenario.atmosphere.coast", nullptr,
                     k_gdi_skip_no_overlay});
    add_row(Scenario{"browser.world3d.legacy", ScenarioKind::kShowcase,
                     detail::kAtmosphereShowcaseMarkLeaf,
                     "browser.world3d.legacy",
                     "world3d.scenario.atmosphere.legacy", nullptr,
                     k_gdi_skip_no_overlay});
    add_row(Scenario{"browser.world3d.globe", ScenarioKind::kShowcase,
                     detail::kAtmosphereShowcaseMarkLeaf,
                     "browser.world3d.globe",
                     "world3d.scenario.atmosphere.globe", nullptr,
                     k_gdi_skip_no_overlay});
  });
}

}  // namespace app

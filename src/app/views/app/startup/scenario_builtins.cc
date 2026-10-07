// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/scenario.h"

#include <mutex>

#include "app/views/browser/plugin/report_suite.h"
#include "app/views/il.runtime/backend/horizon/atom/mark.h"

namespace app {
namespace {

// Chrome / 2D harness: pin Scene3dEngine::kGdi so leftover FlyCube env does
// not start a Display thread. Not used for Scene3D GIS payloads.
constexpr LaunchPolicy k_gdi_skip_overlay{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, true};
constexpr LaunchPolicy k_gdi_product_horizon{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, true};
constexpr LaunchPolicy k_gdi_harness{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, false};
// Scene3D product / atmosphere: FlyCube RHI (never Scene3dEngine::kGdi).
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
    // Browser family — map2d / world3d share one IR shape (horizon→view→plugin);
    // *.browse are stress integration; sibling payloads are carto / atmosphere.
    add_row(Scenario{"browser.map2d.browse", detail::kHarnessMarkLeaf,
                     "browser.map2d.browse", nullptr, nullptr,
                     k_gdi_skip_overlay});
    add_row(Scenario{"browser.map2d.china", detail::kMap2dMarkLeaf,
                     "browser.map2d.china", "map2d.scenario.china", nullptr,
                     k_gdi_skip_overlay});
    add_row(Scenario{"browser.map2d.align", detail::kMap2dMarkLeaf,
                     "browser.map2d.align", "map2d.scenario.align", nullptr,
                     k_gdi_skip_overlay});
    add_row(Scenario{"browser.map2d.orthogrid", detail::kMap2dMarkLeaf,
                     "browser.map2d.orthogrid", "map2d.scenario.orthogrid",
                     nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"browser.world3d.browse", detail::kBrowse3dMarkLeaf,
                     "browser.world3d.browse", nullptr, nullptr, k_browse_3d});
    // Atmosphere suites borrow shell FlyCube (rhi_device). kGdi policy forced
    // Scene3dEngine::kGdi over SCENE3D_ENGINE=flycube → device-missing (#5).
    add_row(Scenario{"browser.world3d.land", detail::kAtmosphereMarkLeaf,
                     "browser.world3d.land",
                     "world3d.scenario.atmosphere.land", nullptr, k_browse_3d});
    add_row(Scenario{"browser.world3d.ocean", detail::kAtmosphereMarkLeaf,
                     "browser.world3d.ocean",
                     "world3d.scenario.atmosphere.ocean", nullptr, k_browse_3d});
    add_row(Scenario{"browser.world3d.full", detail::kAtmosphereMarkLeaf,
                     "browser.world3d.full",
                     "world3d.scenario.atmosphere.full", nullptr, k_browse_3d});
    add_row(Scenario{"browser.world3d.coast", detail::kAtmosphereMarkLeaf,
                     "browser.world3d.coast",
                     "world3d.scenario.atmosphere.coast", nullptr, k_browse_3d});
    add_row(Scenario{"browser.world3d.legacy", detail::kAtmosphereMarkLeaf,
                     "browser.world3d.legacy",
                     "world3d.scenario.atmosphere.legacy", nullptr,
                     k_browse_3d});
    add_row(Scenario{"browser.world3d.globe", detail::kAtmosphereMarkLeaf,
                     "browser.world3d.globe",
                     "world3d.scenario.atmosphere.globe", nullptr, k_browse_3d});
    // Cross-cutting browser integration (HWND / console / digitize / GPU PE).
    add_row(Scenario{"browser.harness", detail::kHarnessMarkLeaf,
                     "browser.harness", nullptr, nullptr, k_gdi_harness});
    add_row(Scenario{"self_test", detail::kHarnessMarkLeaf, "browser.harness",
                     nullptr, nullptr, k_gdi_harness});
    add_row(Scenario{"browser.console", detail::kHarnessMarkLeaf,
                     "browser.console", nullptr, nullptr, k_gdi_harness});
    add_row(Scenario{"browser.input", detail::kInputMarkLeaf, "browser.input",
                     nullptr, nullptr, k_gdi_skip_overlay});
    // browser.gpu is SmartGisRender.exe-only (no Views ScenarioRegistry row).
    add_row(Scenario{"ui.shell", detail::kUiMarkLeaf, "ui.shell", nullptr,
                     nullptr, k_gdi_product_horizon});
    add_row(Scenario{"ui.data", detail::kUiMarkLeaf, "ui.data", nullptr,
                     nullptr, k_gdi_product_horizon});
    add_row(Scenario{"ui.scene", detail::kUiMarkLeaf, "ui.scene", nullptr,
                     nullptr, k_ui_scene});
    add_row(Scenario{"ui.catalog", detail::kUiMarkLeaf, "ui.catalog", nullptr,
                     nullptr, k_gdi_product_horizon});
    // ui.interact* share ui.interact.host.il chrome; gesture body is selected
    // by UI_INTERACT_GESTURE_SCRIPT (product / smoke / combo) or OS inject.
    add_row(Scenario{"ui.interact", detail::kUiMarkLeaf, "ui.interact.host",
                     nullptr, nullptr, k_ui_interact});
    add_row(Scenario{"ui.interact.os", detail::kUiMarkLeaf, "ui.interact.host",
                     nullptr, nullptr, k_ui_interact});
    add_row(Scenario{"ui.interact.smoke", detail::kUiMarkLeaf,
                     "ui.interact.host", nullptr, nullptr,
                     k_gdi_product_horizon});
    add_row(Scenario{"ui.interact.combo", detail::kUiMarkLeaf,
                     "ui.interact.host", nullptr, nullptr,
                     k_gdi_product_horizon});
    // Plugin family — bodies live in product packs; chrome only dispatches.
    add_row(Scenario{"plugin.world3d", detail::kPluginMarkLeaf, "plugin.world3d",
                     "world3d.scenario.showcase", nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.world3d.preview", detail::kPluginMarkLeaf,
                     "plugin.world3d.preview", "world3d.scenario.world_preview",
                     nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.print", detail::kPluginMarkLeaf, "plugin.print",
                     "map2d.scenario.print", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.orthogrid", detail::kPluginMarkLeaf,
                     "plugin.orthogrid", "orthogrid.scenario.showcase", nullptr,
                     k_gdi_skip_overlay});
    add_row(Scenario{"plugin.orthogrid3d", detail::kPluginMarkLeaf,
                     "plugin.orthogrid3d", "orthogrid3d.scenario.showcase",
                     nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.traffic", detail::kPluginMarkLeaf, "plugin.traffic",
                     "traffic.scenario.showcase", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.flood", detail::kPluginMarkLeaf, "plugin.flood",
                     "flood.scenario.showcase", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.stormsurge", detail::kPluginMarkLeaf,
                     "plugin.stormsurge", "stormsurge.scenario.showcase",
                     nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.mine", detail::kPluginMarkLeaf, "plugin.mine",
                     "mine.scenario.showcase", nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.geochem", detail::kPluginMarkLeaf, "plugin.geochem",
                     "geochem.scenario.showcase", nullptr, k_gdi_skip_overlay});
    add_row(Scenario{"plugin.report", detail::kPluginMarkLeaf, "plugin.report",
                     nullptr, &run_report_suite, k_gdi_skip_overlay});
  });
}

}  // namespace app

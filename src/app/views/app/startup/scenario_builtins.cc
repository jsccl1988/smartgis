// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/scenario.h"

#include <mutex>

#include "app/views/app/startup/atmosphere_showcase_run.h"
#include "app/views/app/startup/map2d_showcase_run.h"
#include "app/views/browser/plugin/harness/report_suite.h"
#include "app/views/app/startup/shell_runtime.h"

namespace app {
namespace {

// Chrome / 2D harness: pin Scene3dEngine::kGdi so leftover FlyCube env does
// not start a Display thread. Soft MapIR->HDC is gone — map2d carto SoT is
// FlyCube MapPass (same as bare SmartGIS.exe). ContentMapView faces stay for
// interact / self-test chrome that must not open a 2D Display thread.
constexpr LaunchPolicy k_content_map_skip_catalog{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, false};
// Browse stress: ContentMapView + MapIR→HDC overlay for HWND BitBlt.
constexpr LaunchPolicy k_browse_map2d_gdi{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, true, true};
constexpr LaunchPolicy k_map2d_flycube_skip_catalog{
    Scene3dStartup::kGdi, Map2dStartup::kProduct, true, false};
// Plugin cartography: same face as bare SmartGIS.exe. kGdi Scene3d during
// shell init heap-corrupts the next EXE alloc (HostCapabilities).
constexpr LaunchPolicy k_plugin_product{
    Scene3dStartup::kFlyCube, Map2dStartup::kProduct, true, false};
constexpr LaunchPolicy k_content_map_horizon{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, false};
// UI shell chrome + china map hole: FlyCube MapPass (same carto as product /
// map2d.china). ContentGdi left a black DXGI hole despite Layers checked.
constexpr LaunchPolicy k_ui_shell_flycube{
    Scene3dStartup::kGdi, Map2dStartup::kProduct, false, false};
constexpr LaunchPolicy k_content_map_harness{
    Scene3dStartup::kGdi, Map2dStartup::kContentGdi, false, false};
// Scene3D product / atmosphere: FlyCube on both faces. ContentMapView on
// Map Edit StartRenderProcess's during shell init and heap-corrupts PluginHost.
constexpr LaunchPolicy k_plugin_scene3d{
    Scene3dStartup::kFlyCube, Map2dStartup::kFlyCube2d, true, false};
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
    // Browse stress + HWND BitBlt / motion_gate need ContentMapView + GDI
    // overlay (prepare_env FORCE_*). Product FlyCube Present smears under
    // pan so unique-frame gates stay stuck at 1.
    add_row(Scenario{"browser.map2d.browse", content::detail::kHarnessMarkLeaf,
                     "browser.map2d.browse", nullptr, nullptr,
                     k_browse_map2d_gdi});
    // suite_id prefers IL; Debug CPython stub falls through to C++ run.
    add_row(Scenario{"browser.map2d.china", content::detail::kMap2dMarkLeaf,
                     "browser.map2d.china", nullptr, run_map2d_china_showcase,
                     k_map2d_flycube_skip_catalog});
    add_row(Scenario{"browser.map2d.align", content::detail::kMap2dMarkLeaf,
                     "browser.map2d.align", nullptr, run_map2d_align_showcase,
                     k_map2d_flycube_skip_catalog});
    add_row(Scenario{"browser.map2d.orthogrid", content::detail::kMap2dMarkLeaf,
                     "browser.map2d.orthogrid", nullptr,
                     run_map2d_orthogrid_showcase, k_map2d_flycube_skip_catalog});
    add_row(Scenario{"browser.world3d.browse", content::detail::kBrowse3dMarkLeaf,
                     "browser.world3d.browse", nullptr, nullptr, k_browse_3d});
    // Atmosphere suites borrow shell FlyCube (rhi_device). kGdi policy forced
    // Scene3dEngine::kGdi over SCENE3D_ENGINE=flycube → device-missing (#5).
    // land / ocean / coast looks run inside browser.world3d.full (one process).
    // IL script when CPython is ready; otherwise C++ land→ocean→coast→full.
    // Keep plugin_command null so a stub/gate-only command cannot exit 0
    // without BMPs (same pattern as browser.map2d.china).
    add_row(Scenario{"browser.world3d.full", content::detail::kAtmosphereMarkLeaf,
                     "browser.world3d.full", nullptr,
                     run_atmosphere_full_showcase, k_browse_3d});
    // suite_id prefers IL; Debug CPython stub falls through to C++ run
    // (gate-only plugin_command would exit 0 without a BMP / timed present).
    add_row(Scenario{"browser.world3d.legacy", content::detail::kAtmosphereMarkLeaf,
                     "browser.world3d.legacy", nullptr,
                     run_atmosphere_legacy_showcase, k_browse_3d});
    add_row(Scenario{"browser.world3d.globe", content::detail::kAtmosphereMarkLeaf,
                     "browser.world3d.globe",
                     "world3d.scenario.atmosphere.globe", nullptr, k_browse_3d});
    // Planar DEM→cloud ladder. Script owns the stages; the command is the
    // same fallback the other atmosphere rows keep.
    add_row(Scenario{"browser.world3d.china_stack", content::detail::kAtmosphereMarkLeaf,
                     "browser.world3d.china_stack",
                     "world3d.scenario.atmosphere.china_stack", nullptr,
                     k_browse_3d});
    // Cross-cutting browser integration (HWND / console / digitize / GPU PE).
    add_row(Scenario{"browser.harness", content::detail::kHarnessMarkLeaf,
                     "browser.harness", nullptr, nullptr,
                     k_content_map_harness});
    add_row(Scenario{"self_test", content::detail::kHarnessMarkLeaf, "browser.harness",
                     nullptr, nullptr, k_content_map_harness});
    add_row(Scenario{"browser.console", content::detail::kHarnessMarkLeaf,
                     "browser.console", nullptr, nullptr,
                     k_content_map_harness});
    add_row(Scenario{"browser.input", content::detail::kInputMarkLeaf, "browser.input",
                     nullptr, nullptr, k_content_map_skip_catalog});
    // China seed + digitize + one orbit. Same ContentMapView face as harness.
    add_row(Scenario{"browser.campaign", content::detail::kCampaignMarkLeaf,
                     "browser.campaign", nullptr, nullptr,
                     k_content_map_harness});
    // browser.gpu is SmartGisRender.exe-only (no Views ScenarioRegistry row).
    // suite_id prefers IL; Debug CPython stub falls through to C++ capture.
    add_row(Scenario{"ui.shell", content::detail::kUiMarkLeaf, "ui.shell", nullptr,
                     run_ui_shell_showcase, k_ui_shell_flycube});
    add_row(Scenario{"ui.data", content::detail::kUiMarkLeaf, "ui.data", nullptr,
                     run_ui_data_showcase, k_ui_shell_flycube});
    add_row(Scenario{"ui.scene", content::detail::kUiMarkLeaf, "ui.scene", nullptr,
                     nullptr, k_ui_scene});
    add_row(Scenario{"ui.catalog", content::detail::kUiMarkLeaf, "ui.catalog", nullptr,
                     run_ui_catalog_showcase, k_ui_shell_flycube});
    // Plugins dock only. Index 12 matches shell_layout_composer add order
    // (Tools … Atmosphere, then Plugins). Own mark leaf so ui.catalog
    // captures do not clobber it.
    add_row(Scenario{"ui.plugins", L"ui-plugins-mark.txt", "ui.plugins",
                     nullptr, nullptr, k_ui_shell_flycube});
    // ui.interact* share ui.interact.host.py chrome. combo keeps the ContentGdi
    // horizon policy and also runs the former smoke gestures.
    add_row(Scenario{"ui.interact", content::detail::kUiMarkLeaf, "ui.interact.host",
                     nullptr, nullptr, k_ui_interact});
    add_row(Scenario{"ui.interact.os", content::detail::kUiMarkLeaf, "ui.interact.host",
                     nullptr, nullptr, k_ui_interact});
    add_row(Scenario{"ui.interact.combo", content::detail::kUiMarkLeaf,
                     "ui.interact.host", nullptr, nullptr,
                     k_content_map_horizon});
    // Plugin family — bodies live in product packs; chrome only dispatches.
    add_row(Scenario{"plugin.world3d", content::detail::kPluginMarkLeaf, "plugin.world3d",
                     nullptr, nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.world3d.preview", content::detail::kPluginMarkLeaf,
                     "plugin.world3d.preview", "world3d.scenario.world_preview",
                     nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.print", content::detail::kPluginMarkLeaf, "plugin.print",
                     nullptr, nullptr, k_plugin_product});
    add_row(Scenario{"plugin.orthogrid", content::detail::kPluginMarkLeaf,
                     "plugin.orthogrid", nullptr, nullptr, k_plugin_product});
    add_row(Scenario{"plugin.orthogrid3d", content::detail::kPluginMarkLeaf,
                     "plugin.orthogrid3d", nullptr, nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.traffic", content::detail::kPluginMarkLeaf, "plugin.traffic",
                     nullptr, nullptr, k_plugin_product});
    add_row(Scenario{"plugin.flood", content::detail::kPluginMarkLeaf, "plugin.flood",
                     nullptr, nullptr, k_plugin_product});
    add_row(Scenario{"plugin.stormsurge", content::detail::kPluginMarkLeaf,
                     "plugin.stormsurge", nullptr, nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.mine", content::detail::kPluginMarkLeaf, "plugin.mine",
                     nullptr, nullptr, k_plugin_scene3d});
    add_row(Scenario{"plugin.geochem", content::detail::kPluginMarkLeaf, "plugin.geochem",
                     nullptr, nullptr, k_plugin_product});
    add_row(Scenario{"plugin.report", content::detail::kPluginMarkLeaf, "plugin.report",
                     nullptr, &run_report_suite, k_plugin_product});
  });
}

}  // namespace app

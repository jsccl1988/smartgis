// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/cmdline/views_launch_options.h"

#include <CLI/CLI.hpp>

#include <string>
#include <string_view>

namespace app {
namespace {

content::ProcessType process_type_from_string(const std::string& value) {
  if (value == "renderer") {
    return content::ProcessType::kRenderer;
  }
  if (value == "gpu") {
    return content::ProcessType::kGpu;
  }
  if (value == "utility") {
    return content::ProcessType::kUtility;
  }
  return content::ProcessType::kBrowser;
}

AtmosphereShowcaseMode showcase_from_string(const std::string& value) {
  if (value == "land") {
    return AtmosphereShowcaseMode::kLand;
  }
  if (value == "ocean") {
    return AtmosphereShowcaseMode::kOcean;
  }
  if (value == "full") {
    return AtmosphereShowcaseMode::kFull;
  }
  if (value == "coast") {
    return AtmosphereShowcaseMode::kCoast;
  }
  if (value == "legacy" || value == "stereo") {
    return AtmosphereShowcaseMode::kLegacy;
  }
  if (value == "globe" || value == "earth") {
    return AtmosphereShowcaseMode::kGlobe;
  }
  return AtmosphereShowcaseMode::kNone;
}

Map2dShowcaseMode map2d_showcase_from_string(const std::string& value) {
  if (value == "china") {
    return Map2dShowcaseMode::kChina;
  }
  if (value == "align") {
    return Map2dShowcaseMode::kAlign;
  }
  if (value == "orthogrid" || value == "baogrid") {
    return Map2dShowcaseMode::kOrthogrid;
  }
  return Map2dShowcaseMode::kNone;
}

UiShowcaseMode ui_showcase_from_string(const std::string& value) {
  if (value == "shell") {
    return UiShowcaseMode::kShell;
  }
  if (value == "data") {
    return UiShowcaseMode::kData;
  }
  if (value == "scene" || value == "scene3d") {
    return UiShowcaseMode::kScene;
  }
  if (value == "catalog") {
    return UiShowcaseMode::kCatalog;
  }
  if (value == "interact") {
    return UiShowcaseMode::kInteract;
  }
  return UiShowcaseMode::kNone;
}

}  // namespace

std::string normalize_plugin_showcase_id(std::string_view value) {
  if (value.empty()) {
    return {};
  }
  if (value == "dem") {
    return "world3d";
  }
  if (value == "baogrid") {
    return "orthogrid";
  }
  if (value == "hexgrid") {
    return "orthogrid3d";
  }
  return std::string(value);
}

const char* atmosphere_showcase_name(AtmosphereShowcaseMode mode) {
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      return "land";
    case AtmosphereShowcaseMode::kOcean:
      return "ocean";
    case AtmosphereShowcaseMode::kFull:
      return "full";
    case AtmosphereShowcaseMode::kCoast:
      return "coast";
    case AtmosphereShowcaseMode::kLegacy:
      return "legacy";
    case AtmosphereShowcaseMode::kGlobe:
      return "globe";
    case AtmosphereShowcaseMode::kNone:
    default:
      return "none";
  }
}

const char* map2d_showcase_name(Map2dShowcaseMode mode) {
  switch (mode) {
    case Map2dShowcaseMode::kChina:
      return "china";
    case Map2dShowcaseMode::kAlign:
      return "align";
    case Map2dShowcaseMode::kOrthogrid:
      return "orthogrid";
    case Map2dShowcaseMode::kNone:
    default:
      return "none";
  }
}

const char* ui_showcase_name(UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kShell:
      return "shell";
    case UiShowcaseMode::kData:
      return "data";
    case UiShowcaseMode::kScene:
      return "scene";
    case UiShowcaseMode::kCatalog:
      return "catalog";
    case UiShowcaseMode::kInteract:
      return "interact";
    case UiShowcaseMode::kNone:
    default:
      return "none";
  }
}

ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv) {
  ViewsLaunchOptions out;
  CLI::App app{"SmartGisViews"};
  app.allow_extras();

  std::string type = "browser";
  std::string showcase;
  std::string map2d_showcase;
  std::string plugin_showcase;
  std::string ui_showcase;
  app.add_option("--type", type, "Process role: browser|renderer|gpu|utility")
      ->capture_default_str();
  app.add_flag("--self-test", out.self_test, "Run Views shell self-test");
  app.add_flag("--self-test-console", out.self_test_console,
               "Run DebugAgent console self-test + console_bench.json");
  app.add_flag("--input-showcase", out.input_showcase,
               "Lean digitize FeatureGeom gate (input_loop)");
  app.add_flag("--browse-showcase", out.browse_showcase,
               "Lean pan/browse/wheel gate (browse_loop)");
  app.add_flag("--debug-console", out.debug_console,
               "Start Debug Agent + allow Debug Console");
  app.add_option("--atmosphere-showcase", showcase,
                 "Atmosphere demo: land|ocean|full|coast|legacy|globe");
  app.add_option("--map2d-showcase", map2d_showcase,
                 "2D map demo: china|align|orthogrid");
  app.add_option("--plugin-showcase", plugin_showcase,
                 "Product plugin sample+viz id "
                 "(world3d|world_preview|print|orthogrid|orthogrid3d|traffic|flood|"
                 "stormsurge|mine|geochem|report; aliases dem/baogrid/hexgrid)");
  std::string plugin_present;
  app.add_option("--plugin-present", plugin_present,
                 "Plugin present surface: main|preview "
                 "(preview → MapPreview / WorldPreview by face)");
  app.add_option("--ui-showcase", ui_showcase,
                 "UI chrome demo: shell|data|scene|catalog|interact");
  app.add_option("--atmosphere-fields", out.atmosphere_fields,
                 "Field ingest spec path[:channel[:time]][,...]");
  app.add_option("--shell-canvas", out.shell_canvas,
                 "Shell canvas backend: gdi|skia");
  app.add_option("--plugins-dir", out.plugins_dir,
                 "Product plugin resource root (default: <exe>/../plugins)");
  app.add_flag("--enable-oop-render", out.enable_oop_render,
               "Start OOP GPU MapContents at Session.init_hosts "
               "(default: defer until first ContentMapView attach)");

  try {
    if (argv && argc > 0) {
      app.parse(argc, argv);
    }
  } catch (const CLI::ParseError& e) {
    out.ok = false;
    out.exit_code = app.exit(e);
    return out;
  }

  out.process_type = process_type_from_string(type);
  // Unknown showcase token → kNone (same as the former hand-rolled parser).
  if (!showcase.empty()) {
    out.atmosphere_showcase = showcase_from_string(showcase);
  }
  if (!map2d_showcase.empty()) {
    out.map2d_showcase = map2d_showcase_from_string(map2d_showcase);
  }
  if (!plugin_showcase.empty()) {
    out.plugin_showcase = normalize_plugin_showcase_id(plugin_showcase);
  }
  if (!plugin_present.empty()) {
    if (plugin_present == "preview" || plugin_present == "map_preview" ||
        plugin_present == "world_preview") {
      out.plugin_present = "preview";
    } else {
      out.plugin_present = "main";
    }
  }
  if (!ui_showcase.empty()) {
    out.ui_showcase = ui_showcase_from_string(ui_showcase);
  }
  return out;
}

}  // namespace app

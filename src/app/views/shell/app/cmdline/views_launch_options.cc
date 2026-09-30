// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/cmdline/views_launch_options.h"

#include <CLI/CLI.hpp>

#include <string>

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
  std::string ui_showcase;
  app.add_option("--type", type, "Process role: browser|renderer|gpu|utility")
      ->capture_default_str();
  app.add_flag("--self-test", out.self_test, "Run Views shell self-test");
  app.add_flag("--self-test-console", out.self_test_console,
               "Run DebugAgent console self-test + console_bench.json");
  app.add_flag("--input-showcase", out.input_showcase,
               "Lean digitize FeatureGeom gate (input_loop)");
  app.add_flag("--debug-console", out.debug_console,
               "Start Debug Agent + allow Debug Console");
  app.add_option("--atmosphere-showcase", showcase,
                 "Atmosphere demo: land|ocean|full|coast");
  app.add_option("--map2d-showcase", map2d_showcase,
                 "2D map demo: china|align|orthogrid");
  app.add_option("--ui-showcase", ui_showcase,
                 "UI chrome demo: shell|data|scene|catalog|interact");
  app.add_option("--atmosphere-fields", out.atmosphere_fields,
                 "Field ingest spec path[:channel[:time]][,...]");
  app.add_option("--shell-canvas", out.shell_canvas,
                 "Shell canvas backend: gdi|skia");

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
  if (!ui_showcase.empty()) {
    out.ui_showcase = ui_showcase_from_string(ui_showcase);
  }
  return out;
}

}  // namespace app

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/cmdline/views_launch_options.h"

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

// Map harness --plugin-showcase=<mode> onto ScenarioRegistry ids.
std::string scenario_from_plugin_showcase(const std::string& mode) {
  if (mode.empty()) {
    return {};
  }
  if (mode == "world3d" || mode == "dem") {
    return "plugin.world3d";
  }
  if (mode == "world_preview" || mode == "preview") {
    return "plugin.world3d.preview";
  }
  if (mode == "print") {
    return "plugin.print";
  }
  if (mode == "orthogrid") {
    return "plugin.orthogrid";
  }
  if (mode == "orthogrid3d") {
    return "plugin.orthogrid3d";
  }
  if (mode == "traffic") {
    return "plugin.traffic";
  }
  if (mode == "flood") {
    return "plugin.flood";
  }
  if (mode == "stormsurge") {
    return "plugin.stormsurge";
  }
  if (mode == "mine") {
    return "plugin.mine";
  }
  if (mode == "geochem") {
    return "plugin.geochem";
  }
  if (mode == "report") {
    return "plugin.report";
  }
  // Allow raw registry ids (plugin.world3d, ui.scene, …).
  return mode;
}

}  // namespace

ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv) {
  ViewsLaunchOptions out;
  CLI::App app{"SmartGisViews"};
  app.allow_extras();

  std::string type = "browser";
  std::string plugin_showcase;
  app.add_option("--type", type, "Process role: browser|renderer|gpu|utility")
      ->capture_default_str();
  app.add_flag("--debug-console", out.debug_console,
               "Start Debug Agent + allow Debug Console");
  app.add_option("--shell-canvas", out.shell_canvas,
                 "Shell canvas backend: gdi|skia");
  app.add_option("--plugins-dir", out.plugins_dir,
                 "Product plugin resource root (default: <exe>/plugins)");
  app.add_flag("--enable-oop-render", out.enable_oop_render,
               "Start OOP GPU GisContents at Session.init_tool_sessions "
               "(default: defer until first ContentMapView attach)");
  // Harness suites still pass --plugin-showcase=…; maps to ScenarioRegistry.
  app.add_option("--plugin-showcase", plugin_showcase,
                 "Harness plugin scenario (e.g. world3d → plugin.world3d)");

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
  out.scenario_id = scenario_from_plugin_showcase(plugin_showcase);
  return out;
}

}  // namespace app

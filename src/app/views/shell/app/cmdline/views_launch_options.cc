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

ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv) {
  ViewsLaunchOptions out;
  CLI::App app{"SmartGisViews"};
  app.allow_extras();

  std::string type = "browser";
  std::string showcase;
  app.add_option("--type", type, "Process role: browser|renderer|gpu|utility")
      ->capture_default_str();
  app.add_flag("--self-test", out.self_test, "Run Views shell self-test");
  app.add_flag("--self-test-console", out.self_test_console,
               "Run DebugAgent console self-test + console_bench.json");
  app.add_flag("--debug-console", out.debug_console,
               "Start Debug Agent + allow Debug Console");
  app.add_option("--atmosphere-showcase", showcase,
                 "Atmosphere demo: land|ocean|full|coast");
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
  return out;
}

}  // namespace app

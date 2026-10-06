// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/capability/plugin_bind.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/harness/self_test/self_test.h"
#include "app/views/harness/showcase/atmosphere/atmosphere_showcase.h"
#include "app/views/harness/showcase/map2d/map2d_showcase.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"

namespace app {
namespace detail {

void bind_plugin(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  out->map2d_run = [b](const std::string& mode) {
    Map2dShowcaseMode m = Map2dShowcaseMode::kNone;
    if (mode == "china") {
      m = Map2dShowcaseMode::kChina;
    } else if (mode == "align") {
      m = Map2dShowcaseMode::kAlign;
    } else if (mode == "orthogrid") {
      m = Map2dShowcaseMode::kOrthogrid;
    } else {
      return false;
    }
    return map2d_showcase_body(*b, m) == 0;
  };
  out->atmosphere_run = [b](const std::string& mode) {
    AtmosphereShowcaseMode m = AtmosphereShowcaseMode::kNone;
    if (mode == "land") {
      m = AtmosphereShowcaseMode::kLand;
    } else if (mode == "ocean") {
      m = AtmosphereShowcaseMode::kOcean;
    } else if (mode == "full") {
      m = AtmosphereShowcaseMode::kFull;
    } else if (mode == "coast") {
      m = AtmosphereShowcaseMode::kCoast;
    } else if (mode == "globe" || mode == "earth") {
      m = AtmosphereShowcaseMode::kGlobe;
    } else {
      return false;
    }
    return atmosphere_showcase_body(*b, m) == 0;
  };
  out->run_processing = [b](const std::string& id, const std::string& args) {
    PluginShell* shell = b->plugins();
    if (!shell || id.empty() || !shell->host()) {
      return false;
    }
    return shell->run_processing(id, args);
  };
  out->console_run = [b]() { return console_self_test_body(*b) == 0; };
  out->require_plugins = [b]() {
    if (b->plugins()) {
      (void)b->plugins()->ensure_builtins();
    }
    // Harness print/report: never fail the script on plugin host shape.
    return true;
  };
  out->analysis_set_frame = [b](int index) {
    return b->apply_plugin_frame(index);
  };
  out->analysis_export_frames = [b](const std::string& dir_leaf) {
    return b->export_plugin_frames(dir_leaf);
  };
  out->open_report = [b](const std::string& report_dir) {
    PluginShell* shell = b->plugins();
    if (!shell || !shell->host() || report_dir.empty()) {
      return false;
    }
    plugin::ReportBridge* report = plugin::report_bridge(shell->host());
    if (!report) {
      return false;
    }
    return report->open(report_dir);
  };
  out->post_to_report = [b](const std::string& json) {
    PluginShell* shell = b->plugins();
    if (!shell || !shell->host()) {
      return false;
    }
    plugin::ReportBridge* report = plugin::report_bridge(shell->host());
    if (!report) {
      return false;
    }
    return report->post(json);
  };
}

}  // namespace detail
}  // namespace app

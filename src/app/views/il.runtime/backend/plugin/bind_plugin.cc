// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/plugin/bind_plugin.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/il.runtime/backend/plugin/dispatch.h"
#include "app/views/il.runtime/bind/slots.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"

namespace app {
namespace detail {
namespace {

bool run_named_command(Browser& browser,
                       const std::string& mode,
                       std::initializer_list<std::pair<std::string_view,
                                                       const char*>> table) {
  for (const auto& [name, command_id] : table) {
    if (mode == name) {
      return dispatch_plugin_command(browser, command_id) == 0;
    }
  }
  return false;
}

}  // namespace

void bind_plugin(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out,
      base::tagged_tuple{
          base::tag_resolver<slot_map2d_run> =
              [b](const std::string& mode) {
                return run_named_command(
                    *b, mode,
                    {{"china", "map2d.scenario.china"},
                     {"align", "map2d.scenario.align"},
                     {"orthogrid", "map2d.scenario.orthogrid"}});
              },
          base::tag_resolver<slot_atmosphere_run> =
              [b](const std::string& mode) {
                return run_named_command(
                    *b, mode,
                    {{"land", "world3d.scenario.atmosphere.land"},
                     {"ocean", "world3d.scenario.atmosphere.ocean"},
                     {"full", "world3d.scenario.atmosphere.full"},
                     {"coast", "world3d.scenario.atmosphere.coast"},
                     {"globe", "world3d.scenario.atmosphere.globe"},
                     {"earth", "world3d.scenario.atmosphere.globe"},
                     {"legacy", "world3d.scenario.atmosphere.legacy"}});
              },
          base::tag_resolver<slot_run_plugin_command> =
              [b](const std::string& command_id) {
                if (command_id.empty()) {
                  return false;
                }
                return dispatch_plugin_command(*b, command_id.c_str()) == 0;
              },
          base::tag_resolver<slot_run_processing> =
              [b](const std::string& id, const std::string& args) {
                PluginShell* shell = b->plugins();
                if (!shell || id.empty() || !shell->host()) {
                  return false;
                }
                return shell->run_processing(id, args);
              },
          base::tag_resolver<slot_require_plugins> =
              [b]() {
                if (b->plugins()) {
                  (void)b->plugins()->ensure_builtins();
                }
                // Harness print/report: never fail the script on plugin host shape.
                return true;
              },
          base::tag_resolver<slot_analysis_set_frame> =
              [b](int index) { return b->apply_plugin_frame(index); },
          base::tag_resolver<slot_analysis_export_frames> =
              [b](const std::string& dir_leaf) {
                return b->export_plugin_frames(dir_leaf);
              },
          base::tag_resolver<slot_open_report> =
              [b](const std::string& report_dir) {
                PluginShell* shell = b->plugins();
                if (!shell || !shell->host() || report_dir.empty()) {
                  return false;
                }
                plugin::ReportBridge* report =
                    plugin::report_bridge(shell->host());
                if (!report) {
                  return false;
                }
                return report->open(report_dir);
              },
          base::tag_resolver<slot_post_to_report> =
              [b](const std::string& json) {
                PluginShell* shell = b->plugins();
                if (!shell || !shell->host()) {
                  return false;
                }
                plugin::ReportBridge* report =
                    plugin::report_bridge(shell->host());
                if (!report) {
                  return false;
                }
                return report->post(json);
              },
      });
}

}  // namespace detail
}  // namespace app

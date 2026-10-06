// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/bind_plugin.h"

#include <string>
#include <type_traits>

#include "app/views/app/cmdline/views_launch_options.h"
#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/il.runtime/backend/dispatch.h"
#include "app/views/il.runtime/bind/slots.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"

namespace app {
namespace detail {
namespace {

template <typename Mode>
concept showcase_mode_enum = std::is_enum_v<Mode>;

template <showcase_mode_enum Mode, named_aggregate Pack, typename Body>
bool run_named_showcase(Browser& browser,
                        const std::string& mode,
                        Pack&& table,
                        Body&& body) {
  Mode m = Mode::kNone;
  if (!named_find(std::forward<Pack>(table), mode, &m) || m == Mode::kNone) {
    return false;
  }
  return body(browser, m) == 0;
}

}  // namespace

void bind_plugin(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out,
      base::tagged_tuple{
          base::tag_resolver<slot_map2d_run> =
              [b](const std::string& mode) {
                using namespace base::tuple::literals;
                return run_named_showcase<Map2dShowcaseMode>(
                    *b, mode,
                    base::make_named_tuple(
                        "china"_t = Map2dShowcaseMode::kChina,
                        "align"_t = Map2dShowcaseMode::kAlign,
                        "orthogrid"_t = Map2dShowcaseMode::kOrthogrid),
                    [](Browser& browser, Map2dShowcaseMode mode) {
                      switch (mode) {
                        case Map2dShowcaseMode::kChina:
                          return dispatch_plugin_command(
                              browser, "map2d.scenario.china");
                        case Map2dShowcaseMode::kAlign:
                          return dispatch_plugin_command(
                              browser, "map2d.scenario.align");
                        case Map2dShowcaseMode::kOrthogrid:
                          return dispatch_plugin_command(
                              browser, "map2d.scenario.orthogrid");
                        case Map2dShowcaseMode::kNone:
                          break;
                      }
                      return 1;
                    });
              },
          base::tag_resolver<slot_atmosphere_run> =
              [b](const std::string& mode) {
                using namespace base::tuple::literals;
                return run_named_showcase<AtmosphereShowcaseMode>(
                    *b, mode,
                    base::make_named_tuple(
                        "land"_t = AtmosphereShowcaseMode::kLand,
                        "ocean"_t = AtmosphereShowcaseMode::kOcean,
                        "full"_t = AtmosphereShowcaseMode::kFull,
                        "coast"_t = AtmosphereShowcaseMode::kCoast,
                        "globe"_t = AtmosphereShowcaseMode::kGlobe,
                        "earth"_t = AtmosphereShowcaseMode::kGlobe),
                    [](Browser& browser, AtmosphereShowcaseMode mode) {
                      switch (mode) {
                        case AtmosphereShowcaseMode::kLand:
                          return dispatch_plugin_command(
                              browser, "world3d.scenario.atmosphere.land");
                        case AtmosphereShowcaseMode::kOcean:
                          return dispatch_plugin_command(
                              browser, "world3d.scenario.atmosphere.ocean");
                        case AtmosphereShowcaseMode::kFull:
                          return dispatch_plugin_command(
                              browser, "world3d.scenario.atmosphere.full");
                        case AtmosphereShowcaseMode::kCoast:
                          return dispatch_plugin_command(
                              browser, "world3d.scenario.atmosphere.coast");
                        case AtmosphereShowcaseMode::kGlobe:
                          return dispatch_plugin_command(
                              browser, "world3d.scenario.atmosphere.globe");
                        case AtmosphereShowcaseMode::kLegacy:
                          return dispatch_plugin_command(
                              browser, "world3d.scenario.atmosphere.legacy");
                        case AtmosphereShowcaseMode::kNone:
                          break;
                      }
                      return 1;
                    });
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

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_
#define APP_VIEWS_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

#include <cstddef>
#include <string>

#include "content/app/process_type.h"

namespace app {

// Host-process flags only. Harness scene: --plugin-showcase → |scenario_id|;
// otherwise plugin.json `startup.scenario` is peeked before Browser::init.
struct ViewsLaunchOptions {
  content::ProcessType process_type = content::ProcessType::kBrowser;
  bool debug_console = false;
  // Opt-in OOP GPU child at Session.init_tool_sessions (--enable-oop-render or
  // ENABLE_OOP_RENDER=1). Default is deferred until DrawHost needs it.
  bool enable_oop_render = false;
  bool ok = true;
  int exit_code = 0;
  // ScenarioRegistry id (--plugin-showcase or plugin.json). Empty = product.
  std::string scenario_id;
  // present_dataset surface sticky: main|preview (empty = main).
  std::string plugin_present;
  std::string atmosphere_fields;
  // Empty = unset (caller may fall back to env SHELL_CANVAS).
  std::string shell_canvas;
  // Product plugin resource root. Empty → default <exe>/plugins.
  std::string plugins_dir;
};

inline bool is_harness_launch(const ViewsLaunchOptions& options) {
  return !options.scenario_id.empty();
}

// Touch trailing members so a TU compiled against a truncated ViewsLaunchOptions
// (missing plugins_dir) fails at compile time instead of reading 0xCC past the
// stack object in run_browser_main.
inline constexpr std::size_t k_views_launch_options_plugins_dir_off =
    offsetof(ViewsLaunchOptions, plugins_dir);
inline constexpr std::size_t k_views_launch_options_tail_off =
    offsetof(ViewsLaunchOptions, plugins_dir);
static_assert(k_views_launch_options_plugins_dir_off > 0);
static_assert(k_views_launch_options_tail_off >=
              k_views_launch_options_plugins_dir_off);

// CLI11 parse of argc/argv (wide). Host flags only. On --help / parse error:
// ok=false and exit_code set for wWinMain to return directly.
ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv);

}  // namespace app

#endif  // APP_VIEWS_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

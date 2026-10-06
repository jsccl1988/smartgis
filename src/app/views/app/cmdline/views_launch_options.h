// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_
#define APP_VIEWS_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "content/app/process_type.h"

namespace app {

// Automated 3D atmosphere demos (distinct from full --self-test shell path).
enum class AtmosphereShowcaseMode {
  kNone,
  kLand,
  kOcean,
  kFull,
  kCoast,
  // Leftover stereo look on Views Scene3D (black clear + hypsometric + labels).
  kLegacy,
  // Google-Earth-like: DEM globe + sat cloud shell + sky atmosphere.
  kGlobe,
};

// Automated 2D map carto demos (MapLibre-like China framing / align Style).
enum class Map2dShowcaseMode {
  kNone,
  kChina,
  // Shared StyleDocument still vs MapLibre Native headless
  // (third_party/maplibre/example/style_align.json).
  kAlign,
  // Four boundary curves → Dirichlet Laplace orthogrid → line mesh + BMP.
  kOrthogrid,
};

// Shell horizon capture for ui_shell_loop / --ui-showcase=shell (distinct from --self-test).
enum class UiShowcaseMode {
  kNone,
  kShell,     // Map Edit tab + dark horizon BMP
  kData,      // Map Data tab
  kScene,     // Scene3D tab
  kCatalog,   // Catalog Maps page + Map tab
  kInteract,  // Cycle Map→Data→Scene→Map then capture
};

// Parsed Views PE switches. Pure data — no HWND / Browser.
// Harness / showcase / self-test collapse to |scenario_id| (registry). Empty
// scenario_id is the interactive product (run_loop).
struct ViewsLaunchOptions {
  content::ProcessType process_type = content::ProcessType::kBrowser;
  bool debug_console = false;
  // Opt-in OOP GPU child at Session.init_hosts (--enable-oop-render or
  // ENABLE_OOP_RENDER=1). Default is deferred until DrawHost needs it.
  bool enable_oop_render = false;
  bool ok = true;
  int exit_code = 0;
  // Registered Scenario id, or empty for the product main path.
  std::string scenario_id;
  // present_dataset surface sticky: main|preview (empty = main).
  std::string plugin_present;
  std::string atmosphere_fields;
  // Empty = unset (caller may fall back to env SHELL_CANVAS).
  std::string shell_canvas;
  // Product plugin resource root. Empty → default <exe>/../plugins.
  std::string plugins_dir;
};

inline bool is_harness_launch(const ViewsLaunchOptions& options) {
  return !options.scenario_id.empty();
}

// Touch trailing members so a TU compiled against a truncated ViewsLaunchOptions
// (missing plugin_present / plugins_dir) fails at compile time instead of
// reading 0xCC past the stack object in run_browser_main.
inline constexpr std::size_t k_views_launch_options_plugins_dir_off =
    offsetof(ViewsLaunchOptions, plugins_dir);
inline constexpr std::size_t k_views_launch_options_tail_off =
    offsetof(ViewsLaunchOptions, plugins_dir);
static_assert(k_views_launch_options_plugins_dir_off > 0);
static_assert(k_views_launch_options_tail_off >= k_views_launch_options_plugins_dir_off);

// CLI11 parse of argc/argv (wide). On --help / parse error: ok=false and
// exit_code set for wWinMain to return directly.
ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv);

const char* atmosphere_showcase_name(AtmosphereShowcaseMode mode);
const char* map2d_showcase_name(Map2dShowcaseMode mode);
const char* ui_showcase_name(UiShowcaseMode mode);

// Canonicalize --plugin-showcase aliases. Empty input → empty.
std::string normalize_plugin_showcase_id(std::string_view value);

}  // namespace app

#endif  // APP_VIEWS_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

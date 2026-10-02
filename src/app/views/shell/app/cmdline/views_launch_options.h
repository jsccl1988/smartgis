// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_
#define APP_VIEWS_SHELL_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

#include <string>

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

  // Product plugin sample+viz (--plugin-showcase=...|geochem|mine).
enum class PluginShowcaseMode {
  kNone,
  kWorld3d,
  kPrint,
  kOrthogrid,
  kOrthogrid3d,
  kTraffic,
  kFlood,
  kStormSurge,
  kMine,
  kGeochem,
};

// Shell chrome capture for ui_shell_loop / --ui-showcase=shell (distinct from --self-test).
enum class UiShowcaseMode {
  kNone,
  kShell,     // Map Edit tab + dark chrome BMP
  kData,      // Map Data tab
  kScene,     // Scene3D tab
  kCatalog,   // Catalog Maps page + Map tab
  kInteract,  // Cycle Map→Data→Scene→Map then capture
};

// Parsed Views PE switches. Pure data — no HWND / Browser.
struct ViewsLaunchOptions {
  content::ProcessType process_type = content::ProcessType::kBrowser;
  bool self_test = false;
  // Shorter console-driven shell path (DebugAgent + bench JSON).
  bool self_test_console = false;
  // Lean digitize / FeatureGeom path (testing/tools/harness/shell/input/input_loop.py).
  bool input_showcase = false;
  bool browse_showcase = false;
  bool debug_console = false;
  AtmosphereShowcaseMode atmosphere_showcase = AtmosphereShowcaseMode::kNone;
  Map2dShowcaseMode map2d_showcase = Map2dShowcaseMode::kNone;
  PluginShowcaseMode plugin_showcase = PluginShowcaseMode::kNone;
  UiShowcaseMode ui_showcase = UiShowcaseMode::kNone;
  std::string atmosphere_fields;
  // Empty = unset (caller may fall back to env SMT_SHELL_CANVAS).
  std::string shell_canvas;
  // Product plugin resource root. Empty → default <exe>/../plugins.
  // Each plugin loads from <plugins_dir>/<package>/ (e.g. world3d/).
  std::string plugins_dir;
  // Opt-in OOP GPU child at Session.init_hosts (--enable-oop-render or
  // SMT_ENABLE_OOP_RENDER=1). Default is deferred until MapViewport needs it.
  bool enable_oop_render = false;
  bool ok = true;
  int exit_code = 0;
};

// CLI11 parse of argc/argv (wide). On --help / parse error: ok=false and
// exit_code set for wWinMain to return directly.
ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv);

const char* atmosphere_showcase_name(AtmosphereShowcaseMode mode);
const char* map2d_showcase_name(Map2dShowcaseMode mode);
const char* plugin_showcase_name(PluginShowcaseMode mode);
const char* ui_showcase_name(UiShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

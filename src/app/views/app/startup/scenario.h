// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_STARTUP_SCENARIO_H_
#define APP_VIEWS_APP_STARTUP_SCENARIO_H_

#include <cstddef>
#include <string_view>

namespace app {

class Browser;

// Scene3d engine applied before Browser::init when SCENE3D_ENGINE is unset.
// Product packs own scenario bodies; chrome only pins the engine face.
enum class Scene3dStartup {
  kLeave,
  kGdi,
  kFlyCube,
};

// 2D Map Edit attach / overlay gates. Product vs harness faces.
// kProduct / kFlyCube2d → GpuPresent + present_gpu SoT (no force GDI overlay).
// kContentGdi / kInteract / kPluginScene3d → ContentMapView / GDI faces.
enum class Map2dStartup {
  kProduct,
  kContentGdi,
  kFlyCube2d,
  kInteract,
  kPluginScene3d,
};

// Present / catalog gates for one registered suite entry. Applied before
// Browser::init; suite selection itself is plugin.json `startup.scenario`.
struct LaunchPolicy {
  Scene3dStartup scene3d = Scene3dStartup::kGdi;
  Map2dStartup map2d = Map2dStartup::kContentGdi;
  bool skip_ambox_catalog = true;
  bool force_gdi_overlay = false;
};

// One chrome launch-table entry. |id| matches
// testing/tools/harness/<family>/<id>/suite.json and plugin.json
// startup.scenario. GIS payloads dispatch via |plugin_command| / |suite_id|.
struct Scenario {
  const char* id = nullptr;
  const wchar_t* mark_leaf = nullptr;
  const char* suite_id = nullptr;
  const char* plugin_command = nullptr;
  int (*run)(Browser& browser) = nullptr;
  LaunchPolicy policy;
};

// Registers |scenario| at static-init / first use. Duplicate ids are ignored.
void register_scenario(const Scenario& scenario);

const Scenario* find_scenario(std::string_view id);

// Ensures built-in chrome launch rows are registered.
void ensure_builtin_scenarios();

// Suite script, then optional plugin command, then optional C++ |run|.
int run_scenario(Browser& browser, const Scenario& scenario);

// Lists registered ids into |out| (up to |cap|). Returns count written.
std::size_t list_scenarios(const char** out, std::size_t cap);

}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_SCENARIO_H_

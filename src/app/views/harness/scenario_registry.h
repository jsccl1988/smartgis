// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_SCENARIO_REGISTRY_H_
#define APP_VIEWS_HARNESS_SCENARIO_REGISTRY_H_

#include <cstddef>
#include <string_view>

namespace app {

class Browser;

// Kind of in-process harness path (aligns with suite.json "kind").
enum class ScenarioKind {
  kSelfTest,
  kShowcase,
};

// Scene3d engine applied before Browser::init when SCENE3D_ENGINE is unset.
enum class Scene3dStartup {
  kLeave,
  kGdi,
  kFlyCube,
};

// 2D Map Edit attach / overlay gates. Product vs harness faces.
enum class Map2dStartup {
  kProduct,
  kContentGdi,
  kFlyCube2d,
  kInteract,
  kPluginScene3d,
};

// Present / catalog gates for one registered non-main entry.
struct LaunchPolicy {
  Scene3dStartup scene3d = Scene3dStartup::kGdi;
  Map2dStartup map2d = Map2dStartup::kContentGdi;
  bool skip_ambox_catalog = true;
  bool force_gdi_overlay = false;
};

// One registered suite entry. |id| matches testing/tools/harness/<family>/<id>/suite.json.
struct Scenario {
  const char* id;
  ScenarioKind kind;
  const wchar_t* mark_leaf;  // may be null
  int (*run)(Browser& browser);
  LaunchPolicy policy;
};

// Registers |scenario| at static-init / first use. Duplicate ids are ignored.
void register_scenario(const Scenario& scenario);

const Scenario* find_scenario(std::string_view id);

// Ensures built-in scenarios (browse / console / input) are registered.
void ensure_builtin_scenarios();

// Lists registered ids into |out| (up to |cap|). Returns count written.
std::size_t list_scenarios(const char** out, std::size_t cap);

}  // namespace app

#endif  // APP_VIEWS_HARNESS_SCENARIO_REGISTRY_H_

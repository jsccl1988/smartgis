// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/startup/scenario.h"

#include "app/views/browser/browser.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/views/il.runtime/backend/plugin/dispatch.h"
#include "app/views/il.runtime/codegen/session/run_script.h"
#include "base/process/switches.h"

namespace app {
namespace {

std::mutex& registry_mu() {
  static std::mutex mu;
  return mu;
}

std::unordered_map<std::string, Scenario>& registry_map() {
  static std::unordered_map<std::string, Scenario> map;
  return map;
}

std::vector<std::string>& registry_order() {
  static std::vector<std::string> order;
  return order;
}

}  // namespace

void register_scenario(const Scenario& scenario) {
  if (!scenario.id) {
    return;
  }
  if (!scenario.run && !scenario.suite_id && !scenario.plugin_command) {
    return;
  }
  std::lock_guard<std::mutex> lock(registry_mu());
  auto& map = registry_map();
  auto it = map.find(scenario.id);
  if (it != map.end()) {
    // A peer may register the same id first with an uninitialized LaunchPolicy
    // (poisoned scene3d). Fill from this later complete registration.
    const int scene = static_cast<int>(it->second.policy.scene3d);
    if (scene < 0 || scene > static_cast<int>(Scene3dStartup::kFlyCube)) {
      it->second.policy = scenario.policy;
      if (scenario.mark_leaf) {
        it->second.mark_leaf = scenario.mark_leaf;
      }
    }
    return;
  }
  map.emplace(scenario.id, scenario);
  registry_order().push_back(scenario.id);
}

const Scenario* find_scenario(std::string_view id) {
  ensure_builtin_scenarios();
  std::lock_guard<std::mutex> lock(registry_mu());
  const auto& map = registry_map();
  const auto it = map.find(std::string(id));
  if (it == map.end()) {
    return nullptr;
  }
  return &it->second;
}

std::size_t list_scenarios(const char** out, std::size_t cap) {
  ensure_builtin_scenarios();
  std::lock_guard<std::mutex> lock(registry_mu());
  const auto& order = registry_order();
  const std::size_t n = order.size() < cap ? order.size() : cap;
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = order[i].c_str();
  }
  return n;
}

int run_scenario(Browser& browser, const Scenario& scenario) {
  if (scenario.policy.force_gdi_overlay) {
    base::set_switch("force-gdi-map-overlay", "1");
  }
  if (scenario.suite_id && std::strncmp(scenario.suite_id, "ui.", 3) == 0) {
    base::set_switch("map-identity-hud", "1");
    if (std::strcmp(scenario.suite_id, "ui.scene") == 0) {
      base::set_switch("prefer-gdi-device", "0");
    }
  }
  bool try_il = scenario.suite_id != nullptr;
  if (try_il && scenario.plugin_command &&
      std::strncmp(scenario.plugin_command, "map2d.", 6) == 0) {
    const char* force_il = base::switch_cstr("map2d-showcase-il");
    try_il = force_il && force_il[0] == '1' && force_il[1] == '\0';
  }
  if (try_il) {
    const int rc =
        run_suite_script_rc(browser, scenario.suite_id, scenario.mark_leaf,
                            /*clear_marks=*/true);
    if (rc == 0) {
      return 0;
    }
    if (!scenario.plugin_command && !scenario.run) {
      return rc;
    }
  }
  if (scenario.plugin_command) {
    return detail::dispatch_plugin_command(browser, scenario.plugin_command);
  }
  if (scenario.run) {
    return scenario.run(browser);
  }
  return 1;
}

}  // namespace app

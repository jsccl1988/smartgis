// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/scenario_registry.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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
  if (!scenario.id || !scenario.run) {
    return;
  }
  std::lock_guard<std::mutex> lock(registry_mu());
  auto& map = registry_map();
  auto it = map.find(scenario.id);
  if (it != map.end()) {
    // A peer may register the same id first with an uninitialized LaunchPolicy
    // (0xFDFDFDFD scene3d). Fill from this later complete registration.
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

}  // namespace app

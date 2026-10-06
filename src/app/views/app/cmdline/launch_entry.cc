// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/cmdline/launch_entry.h"

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "base/process/switches.h"

namespace app {
namespace {

std::mutex& bindings_mu() {
  static std::mutex mu;
  return mu;
}

std::vector<LaunchBinding>& bindings() {
  static std::vector<LaunchBinding> list;
  return list;
}

const char* match_atmosphere(const LaunchCli& cli) {
  if (cli.atmosphere.empty()) {
    return nullptr;
  }
  if (cli.atmosphere == "land") {
    return "atmosphere.land";
  }
  if (cli.atmosphere == "ocean") {
    return "atmosphere.ocean";
  }
  if (cli.atmosphere == "full") {
    return "atmosphere.full";
  }
  if (cli.atmosphere == "coast") {
    return "atmosphere.coast";
  }
  if (cli.atmosphere == "legacy" || cli.atmosphere == "stereo") {
    return "atmosphere.legacy";
  }
  if (cli.atmosphere == "globe" || cli.atmosphere == "earth") {
    return "atmosphere.globe";
  }
  return nullptr;
}

const char* match_map2d(const LaunchCli& cli) {
  if (cli.map2d.empty()) {
    return nullptr;
  }
  if (cli.map2d == "china") {
    return "map2d.china";
  }
  if (cli.map2d == "align") {
    return "map2d.align";
  }
  if (cli.map2d == "orthogrid" || cli.map2d == "baogrid") {
    return "map2d.orthogrid";
  }
  return nullptr;
}

const char* match_plugin(const LaunchCli& cli) {
  if (cli.plugin.empty()) {
    return nullptr;
  }
  static thread_local std::string id;
  id = "plugin.";
  id += cli.plugin;
  return id.c_str();
}

const char* match_ui(const LaunchCli& cli) {
  if (cli.ui.empty()) {
    return nullptr;
  }
  if (cli.ui == "shell") {
    return "ui.shell";
  }
  if (cli.ui == "data") {
    return "ui.data";
  }
  if (cli.ui == "scene" || cli.ui == "scene3d") {
    return "ui.scene";
  }
  if (cli.ui == "catalog") {
    return "ui.catalog";
  }
  if (cli.ui == "interact") {
    return "ui.interact";
  }
  return nullptr;
}

const char* match_input(const LaunchCli& cli) {
  return cli.input_showcase ? "input" : nullptr;
}

const char* match_browse(const LaunchCli& cli) {
  if (!cli.browse_showcase) {
    return nullptr;
  }
  if (const char* suite = base::switch_cstr("harness-suite")) {
    if (std::strcmp(suite, "browse.3d") == 0) {
      return "browse.3d";
    }
  }
  if (const char* script = base::switch_cstr("ui-interact-script")) {
    if (script && std::strstr(script, "browse.3d")) {
      return "browse.3d";
    }
  }
  return "browse";
}

const char* match_console(const LaunchCli& cli) {
  return cli.self_test_console ? "console" : nullptr;
}

const char* match_self_test(const LaunchCli& cli) {
  return cli.self_test ? "self_test" : nullptr;
}

void ensure_launch_bindings() {
  static std::once_flag once;
  std::call_once(once, [] {
    // Rank matches former browser_main if-order: atmosphere first, self_test last.
    register_launch_binding({80, &match_atmosphere});
    register_launch_binding({70, &match_map2d});
    register_launch_binding({60, &match_plugin});
    register_launch_binding({50, &match_ui});
    register_launch_binding({40, &match_input});
    register_launch_binding({30, &match_browse});
    register_launch_binding({20, &match_console});
    register_launch_binding({10, &match_self_test});
  });
}

}  // namespace

void register_launch_binding(const LaunchBinding& binding) {
  if (!binding.match) {
    return;
  }
  std::lock_guard<std::mutex> lock(bindings_mu());
  bindings().push_back(binding);
}

std::string resolve_launch_scenario(const LaunchCli& cli) {
  ensure_launch_bindings();
  std::lock_guard<std::mutex> lock(bindings_mu());
  int best_rank = -1;
  std::string best;
  for (const LaunchBinding& binding : bindings()) {
    const char* id = binding.match(cli);
    if (!id || !id[0]) {
      continue;
    }
    if (binding.rank < best_rank) {
      continue;
    }
    best_rank = binding.rank;
    best.assign(id);
  }
  return best;
}

}  // namespace app

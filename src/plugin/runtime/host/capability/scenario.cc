// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/scenario.h"

#include <mutex>
#include <string>
#include <unordered_map>

#include "content/browser/capability/host.h"

namespace plugin {
namespace {

struct Verb {
  ShowcaseVerbFn fn = nullptr;
  std::string default_mode;
};

std::mutex g_mu;
std::unordered_map<std::string, Verb> g_verbs;
int g_last_exit = 1;

}  // namespace

void register_showcase_verb(std::string_view name,
                            ShowcaseVerbFn fn,
                            std::string_view default_mode) {
  if (name.empty() || !fn) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  g_verbs.insert_or_assign(std::string(name),
                           Verb{fn, std::string(default_mode)});
}

bool has_showcase_verb(std::string_view name) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_verbs.contains(std::string(name));
}

std::optional<bool> try_exec_showcase_verb(content::CapabilityHost& host,
                                           std::string_view name,
                                           std::string_view mode) {
  Verb v;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    const auto it = g_verbs.find(std::string(name));
    if (it == g_verbs.end()) {
      return std::nullopt;
    }
    v = it->second;
  }
  std::string resolved(mode);
  if (resolved.empty()) {
    resolved = v.default_mode;
  }
  if (!v.fn) {
    return false;
  }
  return v.fn(host, resolved);
}

void set_harness_scenario_exit(int rc) {
  g_last_exit = rc;
}

int harness_scenario_exit() {
  return g_last_exit;
}

}  // namespace plugin

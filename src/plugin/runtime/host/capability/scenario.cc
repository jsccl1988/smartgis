// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/scenario.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "content/browser/capability/host.h"

namespace plugin {
namespace {

struct ModeBinding {
  std::string mode;
  std::string command_id;
};

struct Op {
  ScenarioOpFn fn = nullptr;
  std::string default_mode;
  std::string fixed_command;
  std::vector<ModeBinding> modes;
};

std::mutex g_mu;
std::unordered_map<std::string, Op> g_ops;
int g_last_exit = 1;

bool run_plugin_command(content::CapabilityHost& host, const char* command_id) {
  return command_id && host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command(command_id);
}

bool exec_op(content::CapabilityHost& host, const Op& op,
             std::string_view mode) {
  std::string resolved(mode);
  if (resolved.empty()) {
    resolved = op.default_mode;
  }
  if (op.fn) {
    return op.fn(host, resolved);
  }
  if (!op.modes.empty()) {
    for (const ModeBinding& m : op.modes) {
      if (m.mode == resolved) {
        return run_plugin_command(host, m.command_id.c_str());
      }
    }
    return false;
  }
  if (!op.fixed_command.empty()) {
    return run_plugin_command(host, op.fixed_command.c_str());
  }
  return false;
}

}  // namespace

void register_scenario_op(std::string_view name,
                          ScenarioOpFn fn,
                          std::string_view default_mode) {
  if (name.empty() || !fn) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  g_ops.insert_or_assign(std::string(name),
                         Op{fn, std::string(default_mode), {}, {}});
}

void register_scenario_command_op(std::string_view name,
                                  std::string_view command_id,
                                  std::string_view default_mode) {
  if (name.empty() || command_id.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  Op op;
  op.default_mode = std::string(default_mode);
  op.fixed_command = std::string(command_id);
  g_ops.insert_or_assign(std::string(name), std::move(op));
}

void register_scenario_mode_op(std::string_view name,
                               std::string_view default_mode,
                               std::initializer_list<ScenarioModeBinding> modes) {
  if (name.empty() || modes.size() == 0) {
    return;
  }
  Op op;
  op.default_mode = std::string(default_mode);
  op.modes.reserve(modes.size());
  for (const ScenarioModeBinding& m : modes) {
    if (!m.mode || !m.command_id) {
      return;
    }
    op.modes.push_back(ModeBinding{m.mode, m.command_id});
  }
  std::lock_guard<std::mutex> lock(g_mu);
  g_ops.insert_or_assign(std::string(name), std::move(op));
}

bool has_scenario_op(std::string_view name) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_ops.contains(std::string(name));
}

std::optional<bool> try_exec_scenario_op(content::CapabilityHost& host,
                                         std::string_view name,
                                         std::string_view mode) {
  Op op;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    const auto it = g_ops.find(std::string(name));
    if (it == g_ops.end()) {
      return std::nullopt;
    }
    op = it->second;
  }
  return exec_op(host, op, mode);
}

void set_harness_scenario_exit(int rc) {
  g_last_exit = rc;
}

int harness_scenario_exit() {
  return g_last_exit;
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/pack_ensure.h"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/contribute_index.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

struct PackEntry {
  std::string prefix;
  // Multiple registrars may share a prefix (product + harness scenario).
  std::vector<PackEnsureFn> ensures;
};

std::mutex g_mu;
std::vector<PackEntry> g_packs;
PackEnableFn g_enable = nullptr;

// Match `prefix` or `prefix.`… . Trailing-dot prefixes (`print.`) keep
// starts_with semantics so orthogrid / print packs stay intentional.
bool prefix_matches(std::string_view command_id, std::string_view prefix) {
  if (prefix.empty() || !command_id.starts_with(prefix)) {
    return false;
  }
  if (prefix.back() == '.') {
    return true;
  }
  return command_id.size() == prefix.size() ||
         command_id[prefix.size()] == '.';
}

const PackEntry* find_best_pack_locked(std::string_view command_id) {
  const PackEntry* best = nullptr;
  size_t best_len = 0;
  for (const PackEntry& e : g_packs) {
    if (e.prefix.empty() || e.ensures.empty()) {
      continue;
    }
    if (!prefix_matches(command_id, e.prefix)) {
      continue;
    }
    if (e.prefix.size() > best_len) {
      best_len = e.prefix.size();
      best = &e;
    }
  }
  return best;
}

}  // namespace

void register_command_pack(std::string_view prefix, PackEnsureFn ensure) {
  if (prefix.empty() || !ensure) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  for (PackEntry& e : g_packs) {
    if (e.prefix != prefix) {
      continue;
    }
    for (PackEnsureFn existing : e.ensures) {
      if (existing == ensure) {
        return;
      }
    }
    e.ensures.push_back(ensure);
    return;
  }
  g_packs.push_back(PackEntry{std::string(prefix), {ensure}});
}

void set_command_pack_enable(PackEnableFn enable) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_enable = enable;
}

bool ensure_for_command(content::PluginHost* host, std::string_view command_id) {
  if (!host || command_id.empty()) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains(command_id)) {
      return true;
    }
  }

  std::vector<PackEnsureFn> ensures;
  PackEnableFn enable = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    if (const PackEntry* pack = find_best_pack_locked(command_id)) {
      ensures = pack->ensures;
    }
    enable = g_enable;
  }

  for (PackEnsureFn fn : ensures) {
    if (!fn) {
      continue;
    }
    (void)fn(host);
    if (tool::CommandCatalog* catalog = host->commands()) {
      if (catalog->contains(command_id)) {
        return true;
      }
    }
  }

  std::string plugin_id;
  if (lookup_contribute_owner(command_id, &plugin_id) && enable) {
    return enable(host, plugin_id);
  }
  return false;
}

}  // namespace plugin

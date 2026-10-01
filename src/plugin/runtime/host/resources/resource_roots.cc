// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/resources/resource_roots.h"

#include <map>
#include <mutex>
#include <string>

namespace plugin {
namespace {

std::mutex g_mu;
std::map<std::string, std::string> g_roots;

std::string join_dir_file(std::string_view dir, std::string_view rel) {
  if (dir.empty() || rel.empty()) {
    return {};
  }
  std::string out(dir);
  const char last = out.back();
  if (last != '/' && last != '\\') {
    out.push_back('/');
  }
  // Allow "tin_loader.ui.xml" or "resources/tin_loader.ui.xml".
  while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
    rel.remove_prefix(1);
  }
  out.append(rel);
  return out;
}

}  // namespace

void set_resource_root(std::string_view plugin_id, std::string directory) {
  if (plugin_id.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  if (directory.empty()) {
    g_roots.erase(std::string(plugin_id));
    return;
  }
  // Normalize trailing separators away for stable joins.
  while (directory.size() > 1 &&
         (directory.back() == '/' || directory.back() == '\\')) {
    directory.pop_back();
  }
  g_roots[std::string(plugin_id)] = std::move(directory);
}

void clear_resource_roots() {
  std::lock_guard<std::mutex> lock(g_mu);
  g_roots.clear();
}

std::string resource_root(std::string_view plugin_id) {
  std::lock_guard<std::mutex> lock(g_mu);
  const auto it = g_roots.find(std::string(plugin_id));
  if (it == g_roots.end()) {
    return {};
  }
  return it->second;
}

std::string resolve_resource(std::string_view plugin_id,
                             std::string_view relative) {
  return join_dir_file(resource_root(plugin_id), relative);
}

}  // namespace plugin

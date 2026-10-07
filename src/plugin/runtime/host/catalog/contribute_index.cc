// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/catalog/contribute_index.h"

#include <mutex>
#include <string>
#include <unordered_map>

#include "plugin/runtime/host/catalog/manifest.h"
#include "plugin/runtime/host/catalog/registry.h"

namespace plugin {
namespace {

std::mutex g_mu;
// Exact command / processing id → plugin id.
std::unordered_map<std::string, std::string> g_exact;
// First segment before '.' (and full id when no '.') → plugin id.
std::unordered_map<std::string, std::string> g_stem;

void index_id_locked(std::string_view id, const std::string& plugin_id) {
  if (id.empty() || plugin_id.empty()) {
    return;
  }
  g_exact.insert_or_assign(std::string(id), plugin_id);
  const size_t dot = id.find('.');
  const std::string stem =
      (dot == std::string_view::npos) ? std::string(id)
                                      : std::string(id.substr(0, dot));
  if (!stem.empty()) {
    g_stem.insert_or_assign(stem, plugin_id);
  }
}

void index_manifest_locked(const Manifest& manifest) {
  if (manifest.id.empty()) {
    return;
  }
  for (const CommandContrib& c : manifest.contributes.commands) {
    index_id_locked(c.id, manifest.id);
  }
  for (const ProcessingContrib& p : manifest.contributes.processing) {
    index_id_locked(p.id, manifest.id);
  }
}

}  // namespace

void clear_contribute_index() {
  std::lock_guard<std::mutex> lock(g_mu);
  g_exact.clear();
  g_stem.clear();
}

void index_manifest_contributes(const Manifest& manifest) {
  std::lock_guard<std::mutex> lock(g_mu);
  index_manifest_locked(manifest);
}

void rebuild_contribute_index(const Registry& registry) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_exact.clear();
  g_stem.clear();
  for (const PluginRecord& rec : registry.list()) {
    if (rec.trust == TrustClass::kDenied) {
      continue;
    }
    index_manifest_locked(rec.manifest);
  }
}

bool lookup_contribute_owner(std::string_view id, std::string* out_plugin_id) {
  if (!out_plugin_id || id.empty()) {
    return false;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  const auto exact = g_exact.find(std::string(id));
  if (exact != g_exact.end()) {
    *out_plugin_id = exact->second;
    return true;
  }
  const size_t dot = id.find('.');
  const std::string stem =
      (dot == std::string_view::npos) ? std::string(id)
                                      : std::string(id.substr(0, dot));
  const auto stem_it = g_stem.find(stem);
  if (stem_it != g_stem.end()) {
    *out_plugin_id = stem_it->second;
    return true;
  }
  return false;
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/pack_enable.h"

#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/catalog/registry.h"

namespace app {
namespace detail {
namespace {

plugin::Registry* g_pack_enable_registry = nullptr;

bool enable_pack_by_id(content::PluginHost* host, std::string_view plugin_id) {
  if (!g_pack_enable_registry || !host || plugin_id.empty()) {
    return false;
  }
  const plugin::PluginRecord* rec = g_pack_enable_registry->find(plugin_id);
  if (!rec || rec->trust == plugin::TrustClass::kDenied) {
    return false;
  }
  if (rec->state == plugin::PluginState::kEnabled) {
    return true;
  }
  return g_pack_enable_registry->set_enabled(plugin_id, true, host);
}

}  // namespace

void install_command_pack_enable(plugin::Registry* registry) {
  g_pack_enable_registry = registry;
  plugin::set_command_pack_enable(enable_pack_by_id);
}

void clear_command_pack_enable() {
  plugin::set_command_pack_enable(nullptr);
  g_pack_enable_registry = nullptr;
}

}  // namespace detail
}  // namespace app

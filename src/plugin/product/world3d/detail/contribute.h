// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_DETAIL_CONTRIBUTE_H_
#define PLUGIN_WORLD3D_DETAIL_CONTRIBUTE_H_

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/plugin_host.h"
#include "tool/command/command.h"

namespace plugin {
namespace detail {

inline constexpr const char* kWorld3dPluginId = "smartgis.world3d";

// One handler / factory contributed under several leftover + product command
// ids (AM, harness, and host_test still address the old stems).
struct CommandAlias {
  const char* id = nullptr;
  const char* title = nullptr;
};

inline bool contribute_command_aliases(
    content::PluginHost* host,
    std::initializer_list<CommandAlias> aliases,
    std::string_view menu_id,
    const tool::CommandHandler& handler) {
  if (!host || !handler) {
    return false;
  }
  for (const CommandAlias& alias : aliases) {
    if (!alias.id || !alias.title) {
      return false;
    }
    if (!host->contribute_command(kWorld3dPluginId, alias.id, alias.title,
                                  menu_id, handler)) {
      return false;
    }
  }
  return true;
}

inline bool contribute_prefixed_commands(
    content::PluginHost* host,
    std::initializer_list<const char*> prefixes,
    std::string_view stem,
    std::string_view title,
    std::string_view menu_id,
    const tool::CommandHandler& handler) {
  if (!host || !handler || stem.empty()) {
    return false;
  }
  for (const char* prefix : prefixes) {
    if (!prefix) {
      return false;
    }
    const std::string id = std::string(prefix) + "." + std::string(stem);
    if (!host->contribute_command(kWorld3dPluginId, id, title, menu_id,
                                  handler)) {
      return false;
    }
  }
  return true;
}

inline bool contribute_processing_aliases(
    content::PluginHost* host,
    std::initializer_list<CommandAlias> aliases,
    const content::ProcessingFactory& factory) {
  if (!host || !factory) {
    return false;
  }
  for (const CommandAlias& alias : aliases) {
    if (!alias.id || !alias.title) {
      return false;
    }
    const content::ProcessingContribution proc = {alias.id, alias.title};
    if (!host->contribute_processing(kWorld3dPluginId, proc, factory)) {
      return false;
    }
  }
  return true;
}

inline bool contribute_prefixed_processing(
    content::PluginHost* host,
    std::initializer_list<const char*> prefixes,
    std::string_view stem,
    std::string_view title,
    const content::ProcessingFactory& factory) {
  if (!host || !factory || stem.empty()) {
    return false;
  }
  for (const char* prefix : prefixes) {
    if (!prefix) {
      return false;
    }
    const std::string id = std::string(prefix) + "." + std::string(stem);
    const content::ProcessingContribution proc = {id, std::string(title)};
    if (!host->contribute_processing(kWorld3dPluginId, proc, factory)) {
      return false;
    }
  }
  return true;
}

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_WORLD3D_DETAIL_CONTRIBUTE_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_CONTRIBUTE_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_CONTRIBUTE_H_

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/plugin_host.h"
#include "tool/command/command.h"

namespace plugin {

// One command / processing id contributed under a product plugin pack.
struct CommandAlias {
  const char* id = nullptr;
  const char* title = nullptr;
};

inline bool contribute_command_aliases(
    content::PluginHost* host, std::string_view plugin_id,
    std::initializer_list<CommandAlias> aliases, std::string_view menu_id,
    const tool::CommandHandler& handler) {
  if (!host || plugin_id.empty() || !handler) {
    return false;
  }
  for (const CommandAlias& alias : aliases) {
    if (!alias.id || !alias.title) {
      return false;
    }
    if (!host->contribute_command(plugin_id, alias.id, alias.title, menu_id,
                                  handler)) {
      return false;
    }
  }
  return true;
}

inline bool contribute_prefixed_commands(
    content::PluginHost* host, std::string_view plugin_id,
    std::initializer_list<const char*> prefixes, std::string_view stem,
    std::string_view title, std::string_view menu_id,
    const tool::CommandHandler& handler) {
  if (!host || plugin_id.empty() || !handler || stem.empty()) {
    return false;
  }
  for (const char* prefix : prefixes) {
    if (!prefix) {
      return false;
    }
    const std::string id = std::string(prefix) + "." + std::string(stem);
    if (!host->contribute_command(plugin_id, id, title, menu_id, handler)) {
      return false;
    }
  }
  return true;
}

inline bool contribute_processing_aliases(
    content::PluginHost* host, std::string_view plugin_id,
    std::initializer_list<CommandAlias> aliases,
    const content::ProcessingFactory& factory) {
  if (!host || plugin_id.empty() || !factory) {
    return false;
  }
  for (const CommandAlias& alias : aliases) {
    if (!alias.id || !alias.title) {
      return false;
    }
    const content::ProcessingContribution proc = {alias.id, alias.title};
    if (!host->contribute_processing(plugin_id, proc, factory)) {
      return false;
    }
  }
  return true;
}

inline bool contribute_prefixed_processing(
    content::PluginHost* host, std::string_view plugin_id,
    std::initializer_list<const char*> prefixes, std::string_view stem,
    std::string_view title, const content::ProcessingFactory& factory) {
  if (!host || plugin_id.empty() || !factory || stem.empty()) {
    return false;
  }
  for (const char* prefix : prefixes) {
    if (!prefix) {
      return false;
    }
    const std::string id = std::string(prefix) + "." + std::string(stem);
    const content::ProcessingContribution proc = {id, std::string(title)};
    if (!host->contribute_processing(plugin_id, proc, factory)) {
      return false;
    }
  }
  return true;
}

// Command catalog + processing factory for one IL / host op id.
inline bool contribute_op(content::PluginHost* host,
                          std::string_view plugin_id, const char* id,
                          const char* title, std::string_view menu_id,
                          const content::ProcessingFactory& factory) {
  if (!host || plugin_id.empty() || !id || !title || !factory) {
    return false;
  }
  return host->contribute_command(
             plugin_id, id, title, menu_id,
             [host, factory](const tool::CommandArgs& args) {
               return factory(host, args.payload);
             }) &&
         host->contribute_processing(plugin_id, {id, title}, factory);
}

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_CONTRIBUTE_H_

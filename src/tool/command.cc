// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/command.h"

namespace tool {

bool CommandCatalog::add(std::string_view id, CommandHandler handler) {
  if (id.empty() || !handler) {
    return false;
  }
  std::string key(id);
  if (handlers_.find(key) != handlers_.end()) {
    return false;
  }
  handlers_.emplace(std::move(key), std::move(handler));
  return true;
}

const CommandHandler* CommandCatalog::find(std::string_view id) const {
  auto it = handlers_.find(std::string(id));
  if (it == handlers_.end()) {
    return nullptr;
  }
  return &it->second;
}

bool CommandCatalog::contains(std::string_view id) const {
  return find(id) != nullptr;
}

void CommandCatalog::for_each(
    const std::function<void(std::string_view id)>& fn) const {
  if (!fn) {
    return;
  }
  for (const auto& kv : handlers_) {
    fn(kv.first);
  }
}

CommandDispatcher::CommandDispatcher(CommandCatalog* catalog)
    : catalog_(catalog) {}

bool CommandDispatcher::execute(std::string_view id, const CommandArgs& args) {
  if (!catalog_) {
    return false;
  }
  const CommandHandler* handler = catalog_->find(id);
  if (!handler) {
    return false;
  }
  return (*handler)(args);
}

}  // namespace tool

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/command/command.h"

#include <map>
#include <string>

namespace tool {

struct CommandCatalog::Impl {
  std::map<std::string, CommandHandler> handlers;
};

CommandCatalog::CommandCatalog() : impl_(std::make_unique<Impl>()) {}

CommandCatalog::~CommandCatalog() = default;

bool CommandCatalog::add(std::string_view id, CommandHandler handler) {
  if (!impl_ || id.empty() || !handler) {
    return false;
  }
  std::string key(id);
  if (impl_->handlers.find(key) != impl_->handlers.end()) {
    return false;
  }
  impl_->handlers.emplace(std::move(key), std::move(handler));
  return true;
}

const CommandHandler* CommandCatalog::find(std::string_view id) const {
  if (!impl_) {
    return nullptr;
  }
  auto it = impl_->handlers.find(std::string(id));
  if (it == impl_->handlers.end()) {
    return nullptr;
  }
  return &it->second;
}

bool CommandCatalog::contains(std::string_view id) const {
  return find(id) != nullptr;
}

void CommandCatalog::for_each(
    const std::function<void(std::string_view id)>& fn) const {
  if (!impl_ || !fn) {
    return;
  }
  for (const auto& kv : impl_->handlers) {
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

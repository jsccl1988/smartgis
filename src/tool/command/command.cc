// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "tool/command/command.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace tool {

struct CommandCatalog::Impl {
  // Ids and handlers are separate allocations. A pair<string, function>
  // put the cross-module std::function in the same buffer as the id, and
  // the next contains() died in _Large_mode_engaged on a smashed string
  // (plugin.report register_report).
  mutable std::mutex mu;
  std::vector<std::string> ids;
  std::vector<CommandHandler> handlers;
};

CommandCatalog::CommandCatalog() : impl_(std::make_unique<Impl>()) {}

CommandCatalog::~CommandCatalog() = default;

bool CommandCatalog::add(std::string_view id, CommandHandler handler) {
  if (!impl_ || id.empty() || !handler) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  for (const std::string& existing : impl_->ids) {
    if (existing == id) {
      return false;
    }
  }
  impl_->ids.emplace_back(id);
  impl_->handlers.push_back(std::move(handler));
  return true;
}

const CommandHandler* CommandCatalog::find(std::string_view id) const {
  if (!impl_ || id.empty()) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  const std::size_t n = std::min(impl_->ids.size(), impl_->handlers.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (impl_->ids[i] == id) {
      return &impl_->handlers[i];
    }
  }
  return nullptr;
}

bool CommandCatalog::contains(std::string_view id) const {
  return find(id) != nullptr;
}

void CommandCatalog::for_each(
    const std::function<void(std::string_view id)>& fn) const {
  if (!impl_ || !fn) {
    return;
  }
  std::vector<std::string> ids;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    ids.reserve(impl_->handlers.size());
    ids = impl_->ids;
  }
  for (const std::string& id : ids) {
    fn(id);
  }
}

CommandDispatcher::CommandDispatcher(CommandCatalog* catalog)
    : catalog_(catalog) {}

bool CommandDispatcher::execute(std::string_view id, const CommandArgs& args) {
  if (!catalog_) {
    return false;
  }
  const CommandHandler* handler = catalog_->find(id);
  if (!handler || !*handler) {
    return false;
  }
  CommandHandler copy = *handler;
  return copy(args);
}

}  // namespace tool

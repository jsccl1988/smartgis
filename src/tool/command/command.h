// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_COMMAND_H_
#define TOOL_COMMAND_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#include "tool/tool_export.h"

// Fire-and-forget shell actions. Not pointer routing and not document writes.
namespace tool {

struct CommandArgs {
  uint32_t view_id = 0;
  std::string_view payload;
};

using CommandHandler = std::function<bool(const CommandArgs&)>;

// String-id → handler map. Methods exported; class not — avoids C4251 on pimpl.
class CommandCatalog {
 public:
  TOOL_EXPORT CommandCatalog();
  TOOL_EXPORT ~CommandCatalog();

  CommandCatalog(const CommandCatalog&) = delete;
  CommandCatalog& operator=(const CommandCatalog&) = delete;

  TOOL_EXPORT bool add(std::string_view id, CommandHandler handler);
  TOOL_EXPORT const CommandHandler* find(std::string_view id) const;
  TOOL_EXPORT bool contains(std::string_view id) const;
  // Visit registered ids in map order. Empty |fn| is a no-op.
  TOOL_EXPORT void for_each(
      const std::function<void(std::string_view id)>& fn) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class CommandDispatcher {
 public:
  TOOL_EXPORT explicit CommandDispatcher(CommandCatalog* catalog);
  TOOL_EXPORT bool execute(std::string_view id, const CommandArgs& args);

 private:
  CommandCatalog* catalog_ = nullptr;
};

}  // namespace tool

#endif  // TOOL_COMMAND_H_

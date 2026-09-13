// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_COMMAND_H_
#define TOOL_COMMAND_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

// Fire-and-forget chrome actions. Not pointer routing and not document writes.
namespace tool {

struct CommandArgs {
  uint32_t view_id = 0;
  std::string_view payload;
};

using CommandHandler = std::function<bool(const CommandArgs&)>;

class CommandCatalog {
 public:
  bool add(std::string_view id, CommandHandler handler);
  const CommandHandler* find(std::string_view id) const;
  bool contains(std::string_view id) const;
  // Visit registered ids in map order. Empty |fn| is a no-op.
  void for_each(const std::function<void(std::string_view id)>& fn) const;

 private:
  std::map<std::string, CommandHandler> handlers_;
};

class CommandDispatcher {
 public:
  explicit CommandDispatcher(CommandCatalog* catalog);
  bool execute(std::string_view id, const CommandArgs& args);

 private:
  CommandCatalog* catalog_ = nullptr;
};

}  // namespace tool

#endif  // TOOL_COMMAND_H_

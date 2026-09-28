// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PLUGIN_SHELL_H_
#define APP_VIEWS_PLUGIN_SHELL_H_

#include <memory>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
class EventBus;
class PluginHost;
}  // namespace content

namespace plugin {
class ProcessingPool;
class Registry;
}  // namespace plugin

namespace tool {
class CommandCatalog;
}  // namespace tool

namespace app {

// Owns the Views-side plugin platform (Registry + PluginHost + pool).
// Lives in a TU that must not include content/public/map_contents.h.
class PluginShell {
 public:
  PluginShell();
  ~PluginShell();

  PluginShell(const PluginShell&) = delete;
  PluginShell& operator=(const PluginShell&) = delete;

  bool init(content::EventBus* events);
  void shutdown();
  bool show_manager(HWND owner);

  content::PluginHost* host() const;
  // Same catalog PluginHost::commands() returns; for Ambox enumeration.
  tool::CommandCatalog* commands() const;
  plugin::Registry* registry() const;
  plugin::ProcessingPool* processing_pool() const;
  void flush_processing_for_test();
  bool run_processing(std::string_view processing_id,
                       std::string_view args_json);
  bool execute(std::string_view command_id);

 private:
  bool start_builtins();

  std::unique_ptr<tool::CommandCatalog> catalog_;
  std::unique_ptr<content::PluginHost> host_;
  std::unique_ptr<plugin::Registry> registry_;
  std::unique_ptr<plugin::ProcessingPool> pool_;
};

}  // namespace app

#endif  // APP_VIEWS_PLUGIN_SHELL_H_

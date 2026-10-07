// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PLUGIN_SHELL_H_
#define APP_VIEWS_PLUGIN_SHELL_H_

#include <memory>
#include <string>
#include <string_view>

namespace content {
class EventBus;
class GisContents;
class PluginHost;
}  // namespace content

namespace plugin {
struct HostCapabilities;
class ProcessingPool;
class PythonRuntime;
class Registry;
class PluginManager;
}  // namespace plugin

namespace tool {
class CommandCatalog;
struct CommandArgs;
}  // namespace tool

namespace app {

// Default <exe>/plugins (out/Debug/plugins or out/Release/plugins).
std::string default_plugins_dir();

// Read plugin.json `startup` without LoadLibrary. Highest `priority` among
// activated packs wins `scenario` / `present` / `fields`.
void peek_plugin_startup(const std::string& plugins_dir,
                         std::string* scenario_id,
                         std::string* plugin_present,
                         std::string* atmosphere_fields);

// Owns the Views-side plugin platform (Registry + CommandCatalog + pool).
// PluginHost lifetime belongs to GisContents; this holds a non-owning pointer
// after attach_gis_contents. Must not include content/public/gis_contents.h.
class PluginShell {
 public:
  PluginShell();
  ~PluginShell();

  PluginShell(const PluginShell&) = delete;
  PluginShell& operator=(const PluginShell&) = delete;

  // Catalog / registry / pool only — no PluginHost yet (Create after catalog).
  bool init(content::EventBus* events);
  // Create-or-bind PluginHost on |contents|; wire capabilities + processing.
  bool attach_gis_contents(content::GisContents* contents);
  // Optional: set before init(). Empty → default <exe>/plugins.
  void set_plugins_dir(std::string path);
  void shutdown();

  content::PluginHost* host() const;
  // Same catalog PluginHost::commands() returns; for Ambox enumeration.
  tool::CommandCatalog* commands() const;
  plugin::Registry* registry() const;
  plugin::ProcessingPool* processing_pool() const;
  plugin::PythonRuntime* python_runtime() const;
  void flush_processing_for_test();
  bool run_processing(std::string_view processing_id,
                       std::string_view args_json);
  bool execute(std::string_view command_id);
  // Same thread as the caller (UI/browser when invoked from chrome). Payload
  // lives in |args|; do not use run_processing for GisScene mutations.
  bool execute(std::string_view command_id, const tool::CommandArgs& args);

  // Ensure CPython is up and host is bound into smartgis.content.host.
  bool ensure_python();
  std::string eval_python(std::string_view code);

  // Scan plugin.json + in-process builtins. Native DLLs load on command /
  // processing / catalog enable — not all packs at once.
  bool ensure_builtins();

  // Scan manifests only (no enable-all). Safe for harness single-pack dispatch.
  bool ensure_discovered();

  // Dynamic pack ensure: in-process prefix table, else Registry/manifest
  // LoadLibrary for the owning plugin. Does not enable every pack.
  bool ensure_command(std::string_view command_id);

  // Product / harness: apply plugin.json `startup` (commands / seed / viewport).
  // Viewport string is `map2d` / `scene3d` / empty (highest `priority` wins).
  bool apply_startup();
  const std::string& startup_viewport() const { return startup_viewport_; }
  const std::string& startup_scenario() const { return startup_scenario_; }

 private:
  void init_python();
  bool start_builtins();
  void install_builtin_resource_roots();
  void install_command_pack_enable();

  std::unique_ptr<tool::CommandCatalog> catalog_;
  // Non-owning; GisContents owns the PluginHost.
  content::PluginHost* host_ = nullptr;
  content::EventBus* events_ = nullptr;
  std::unique_ptr<plugin::Registry, void (*)(plugin::Registry*)> registry_;
  std::unique_ptr<plugin::ProcessingPool> pool_;
  std::unique_ptr<plugin::PythonRuntime> python_;
  std::unique_ptr<plugin::HostCapabilities> capabilities_;
  std::unique_ptr<plugin::PluginManager, void (*)(plugin::PluginManager*)>
      manager_;
  std::string plugins_dir_;
  std::string startup_viewport_;
  std::string startup_scenario_;
  bool shutdown_done_ = false;
  bool builtins_started_ = false;
  bool startup_applied_ = false;
  bool discovered_ = false;
  bool host_attached_ = false;
};

}  // namespace app

#endif  // APP_VIEWS_PLUGIN_SHELL_H_

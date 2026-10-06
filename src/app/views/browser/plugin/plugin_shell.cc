// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/plugin_shell.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#include "app/views/browser/plugin/builtins.h"
#include "app/views/util/exe_sidecar_path.h"
#include "base/trace/event/process_trace.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/native/manager.h"
#include "plugin/runtime/host/native/scan.h"
#include "plugin/runtime/host/processing/processing.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "plugin/runtime/host/catalog/resource_roots.h"
#include "plugin/runtime/python/runtime.h"
#include "tool/command/command.h"
#include "ui/views/kernel/paint/painter_registry.h"

namespace app {

std::string default_plugins_dir() {
  char path[MAX_PATH] = {};
  // <exe_dir>/plugins  (out/Debug/plugins or out/Release/plugins)
  if (!detail::exe_sidecar_path_a(path, MAX_PATH, "plugins")) {
    return {};
  }
  return std::string(path);
}

namespace {

bool command_owned_by(const plugin::PluginRecord& rec, std::string_view id) {
  for (const plugin::CommandContrib& c : rec.manifest.contributes.commands) {
    if (c.id == id) {
      return true;
    }
  }
  for (const plugin::ProcessingContrib& p :
       rec.manifest.contributes.processing) {
    if (p.id == id) {
      return true;
    }
  }
  return false;
}

bool enable_command_owner(plugin::Registry* registry, content::PluginHost* host,
                          std::string_view id) {
  if (!registry || !host || id.empty()) {
    return false;
  }
  for (const plugin::PluginRecord& rec : registry->list()) {
    if (rec.trust == plugin::TrustClass::kDenied) {
      continue;
    }
    if (!command_owned_by(rec, id)) {
      continue;
    }
    if (rec.state == plugin::PluginState::kEnabled) {
      return true;
    }
    return registry->set_enabled(rec.manifest.id, true, host);
  }
  return false;
}

}  // namespace

void peek_plugin_startup(const std::string& plugins_dir,
                         std::string* scenario_id,
                         std::string* plugin_present,
                         std::string* atmosphere_fields) {
  if (scenario_id) {
    scenario_id->clear();
  }
  if (plugin_present) {
    plugin_present->clear();
  }
  if (atmosphere_fields) {
    atmosphere_fields->clear();
  }
  std::string root = plugins_dir.empty() ? default_plugins_dir() : plugins_dir;
  if (root.empty()) {
    return;
  }
  plugin::PluginStartupPeek peek{};
  plugin::peek_plugins_dir_startup(root.c_str(), &peek);
  if (scenario_id && peek.scenario[0]) {
    *scenario_id = peek.scenario;
  }
  if (plugin_present && peek.present[0]) {
    *plugin_present = peek.present;
  }
  if (atmosphere_fields && peek.fields[0]) {
    *atmosphere_fields = peek.fields;
  }
}

PluginShell::PluginShell()
    : registry_(nullptr, &plugin::registry_delete),
      manager_(nullptr, &plugin::plugin_manager_delete) {}

PluginShell::~PluginShell() {
  shutdown();
}

void PluginShell::set_plugins_dir(std::string path) {
  plugins_dir_ = std::move(path);
}

void PluginShell::install_builtin_resource_roots() {
  std::string root = plugins_dir_;
  if (root.empty()) {
    root = default_plugins_dir();
  }
  app::install_builtin_resource_roots(root);
}

void PluginShell::init_python() {
  python_ = std::make_unique<plugin::PythonRuntime>();
  if (!python_->init()) {
    // Soft: product runs without embeddable CPython.
    return;
  }
  plugin::bind_registry_python(registry_.get(), python_.get());
  python_->bind_host(host_.get());
}

bool PluginShell::init(content::EventBus* events) {
  BASE_TRACE_EVENT("PluginShell.init.body", "startup");
  {
    BASE_TRACE_EVENT("PluginHost.create", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell CommandCatalog");
    catalog_ = std::make_unique<tool::CommandCatalog>();
    LOGGING(LOG_INFO, "startup: PluginShell create_plugin_host");
    host_.reset(content::create_plugin_host(catalog_.get(), events, nullptr));
    if (!host_) {
      return false;
    }
    LOGGING(LOG_INFO, "startup: PluginShell HostCapabilities");
    capabilities_ = std::make_unique<plugin::HostCapabilities>();
    capabilities_->attach(host_.get());
  }
  // Keep PainterRegistry out of content/: wire withdraw here only.
  host_->set_ui_withdraw_hook([](std::string_view id) {
    ui::views::PainterRegistry::get().withdraw_plugin(id);
  });
  {
    BASE_TRACE_EVENT("PluginRegistry.setup", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell Registry");
    registry_.reset(plugin::registry_new());
    LOGGING(LOG_INFO, "startup: PluginShell PluginManager");
    manager_.reset(plugin::plugin_manager_new(registry_.get()));
    LOGGING(LOG_INFO, "startup: PluginShell ProcessingPool");
    pool_ = std::make_unique<plugin::ProcessingPool>(
        plugin::ProcessingMode::kThread);
    plugin::attach_host_processing(host_.get(), pool_.get());
  }
  {
    BASE_TRACE_EVENT("PluginResourceRoots", "startup");
    install_builtin_resource_roots();
  }
  // Defer CPython and LoadLibrary builtins until ensure_builtins / ensure_python
  // so Browser::init → first paint is not blocked by plugin DLL enable.
  return true;
}

void PluginShell::shutdown() {
  if (shutdown_done_) {
    return;
  }
  shutdown_done_ = true;
  // Disable plugins before tearing host/registry. Guard against a half-inited
  // or already-freed registry (init failure → unique_ptr reset → ~PluginShell).
  if (registry_ && host_) {
    if (manager_) {
      manager_->destroy_all(host_.get());
    }
    const std::vector<plugin::PluginRecord> records = registry_->list();
    for (const plugin::PluginRecord& rec : records) {
      if (rec.state == plugin::PluginState::kEnabled) {
        registry_->set_enabled(rec.manifest.id, false, host_.get());
      }
    }
  }
  if (python_) {
    plugin::clear_gis_console_bridge();
    plugin::bind_gis_host_for_analysis(nullptr);
    python_->shutdown();
    python_.reset();
  }
  pool_.reset();
  if (registry_) {
    registry_->set_native_starter({}, {}, {});
  }
  manager_.reset();
  if (capabilities_ && host_) {
    capabilities_->detach(host_.get());
  }
  capabilities_.reset();
  host_.reset();
  registry_.reset();
  catalog_.reset();
  plugin::clear_resource_roots();
}

content::PluginHost* PluginShell::host() const {
  return host_.get();
}

tool::CommandCatalog* PluginShell::commands() const {
  return catalog_.get();
}

plugin::Registry* PluginShell::registry() const {
  return registry_.get();
}

plugin::ProcessingPool* PluginShell::processing_pool() const {
  return pool_.get();
}

plugin::PythonRuntime* PluginShell::python_runtime() const {
  return python_.get();
}

void PluginShell::flush_processing_for_test() {
  if (pool_) {
    pool_->flush_for_test();
  }
}

bool PluginShell::run_processing(std::string_view processing_id,
                                  std::string_view args_json) {
  if (!host_ || processing_id.empty()) {
    return false;
  }
  (void)ensure_builtins();
  (void)enable_command_owner(registry_.get(), host_.get(), processing_id);
  // Enqueue on the utility pool, then drain here so callers see completed
  // MapScene mutations before continuing (showcase export / marks).
  if (!host_->run_processing(processing_id, args_json)) {
    std::fprintf(stderr,
                 "plugin_shell: run_processing reject id=%.*s (missing or "
                 "enqueue false)\n",
                 static_cast<int>(processing_id.size()), processing_id.data());
    std::fflush(stderr);
    return false;
  }
  flush_processing_for_test();
  // submit() returns true on enqueue; propagate the factory result after drain.
  if (pool_ && !pool_->last_ok()) {
    const std::string msg = pool_->last_message();
    std::fprintf(stderr, "plugin_shell: run_processing failed id=%.*s msg=%s\n",
                 static_cast<int>(processing_id.size()), processing_id.data(),
                 msg.empty() ? "(empty)" : msg.c_str());
    std::fflush(stderr);
    return false;
  }
  return true;
}

bool PluginShell::execute(std::string_view command_id) {
  return execute(command_id, tool::CommandArgs{});
}

bool PluginShell::execute(std::string_view command_id,
                          const tool::CommandArgs& args) {
  if (!host_ || command_id.empty()) {
    return false;
  }
  (void)ensure_builtins();
  if (host_->execute(command_id, args)) {
    return true;
  }
  if (enable_command_owner(registry_.get(), host_.get(), command_id) &&
      host_->execute(command_id, args)) {
    return true;
  }
  if (manager_ &&
      manager_->dispatch_event(command_id, args.payload) > 0) {
    return true;
  }
  return false;
}

bool PluginShell::ensure_python() {
  (void)ensure_builtins();
  if (!python_) {
    init_python();
  }
  if (!python_ || !python_->is_ready()) {
    return false;
  }
  python_->bind_host(host_.get());
  return true;
}

std::string PluginShell::eval_python(std::string_view code) {
  if (!ensure_python()) {
    return "error: python not ready";
  }
  return python_->eval(code);
}

bool PluginShell::ensure_builtins() {
  if (builtins_started_) {
    return true;
  }
  if (!registry_ || !host_) {
    return false;
  }
  builtins_started_ = true;
  return start_builtins();
}

bool PluginShell::start_builtins() {
  if (!registry_ || !host_) {
    return false;
  }
  std::string root = plugins_dir_;
  if (root.empty()) {
    root = default_plugins_dir();
  }
  if (manager_) {
    (void)manager_->scan_directory(root);
  }
  (void)app::register_builtin_plugins(registry_.get(), host_.get());
  // Do not LoadLibrary every native pack here. Eight GDAL-linked plugin
  // DLLs on debug CRT → STATUS_HEAP_CORRUPTION (0xC0000374) during product
  // startup. Native init happens on execute / processing / catalog enable.
  return true;
}

bool PluginShell::apply_startup() {
  if (!ensure_builtins() || !registry_ || !host_) {
    return false;
  }
  if (startup_applied_) {
    return true;
  }
  startup_applied_ = true;

  int best_viewport = 0;
  int best_scenario = 0;
  bool have_viewport = false;
  bool have_scenario = false;
  startup_viewport_.clear();
  startup_scenario_.clear();

  const std::vector<plugin::PluginRecord> rows = registry_->list();
  for (const plugin::PluginRecord& rec : rows) {
    if (rec.trust == plugin::TrustClass::kDenied) {
      continue;
    }
    const plugin::ManifestStartup& st = rec.manifest.startup;
    if (!st.activate) {
      continue;
    }
    if (!st.viewport.empty() &&
        (!have_viewport || st.priority > best_viewport)) {
      have_viewport = true;
      best_viewport = st.priority;
      startup_viewport_ = st.viewport;
    }
    if (!st.scenario.empty() &&
        (!have_scenario || st.priority > best_scenario)) {
      have_scenario = true;
      best_scenario = st.priority;
      startup_scenario_ = st.scenario;
    }
    if (!st.commands.empty() || !st.seed.empty()) {
      (void)registry_->set_enabled(rec.manifest.id, true, host_.get());
    }
    for (const std::string& cmd : st.commands) {
      if (cmd.empty()) {
        continue;
      }
      (void)host_->execute(cmd, tool::CommandArgs{});
    }
    if (!st.seed.empty()) {
      const int face = st.viewport == "scene3d" ? 1 : 0;
      (void)host_->present_dataset(rec.manifest.id, st.seed, face);
    }
  }
  return true;
}

}  // namespace app

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/plugin_shell.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "app/views/browser/plugin/builtins.h"
#include "app/views/browser/plugin/pack_enable.h"
#include "app/views/browser/plugin/startup_apply.h"
#include "base/trace/event/process_trace.h"
#include "content/public/gis_contents.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/catalog/contribute_index.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "plugin/runtime/host/catalog/resource_roots.h"
#include "plugin/runtime/host/native/manager.h"
#include "plugin/runtime/host/processing/processing.h"
#include "plugin/runtime/python/runtime.h"
#include "tool/command/command.h"
#include "ui/views/kernel/paint/painter_registry.h"

namespace app {

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
  python_->bind_host(host_);
}

bool PluginShell::init(content::EventBus* events) {
  BASE_TRACE_EVENT("PluginShell.init.body", "startup");
  events_ = events;
  {
    BASE_TRACE_EVENT("PluginCatalog.create", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell CommandCatalog");
    catalog_ = std::make_unique<tool::CommandCatalog>();
  }
  {
    BASE_TRACE_EVENT("PluginRegistry.setup", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell Registry");
    registry_.reset(plugin::registry_new());
    LOGGING(LOG_INFO, "startup: PluginShell PluginManager");
    manager_.reset(plugin::plugin_manager_new(registry_.get()));
    LOGGING(LOG_INFO, "startup: PluginShell ProcessingPool");
    pool_ = std::make_unique<plugin::ProcessingPool>(
        plugin::ProcessingMode::kThread);
  }
  {
    BASE_TRACE_EVENT("PluginResourceRoots", "startup");
    install_builtin_resource_roots();
  }
  install_command_pack_enable();
  // PluginHost is created on GisContents via attach_gis_contents (after
  // ensure_gis_contents). Defer CPython / LoadLibrary until ensure_*.
  return true;
}

bool PluginShell::attach_gis_contents(content::GisContents* contents) {
  if (!contents || !catalog_) {
    return false;
  }
  if (host_attached_ && host_) {
    return true;
  }
  BASE_TRACE_EVENT("PluginHost.attach", "startup");
  LOGGING(LOG_INFO, "startup: PluginShell ensure_plugin_host");
  content::PluginHost* host =
      contents->ensure_plugin_host(catalog_.get(), events_);
  if (!host) {
    return false;
  }
  host_ = host;
  LOGGING(LOG_INFO, "startup: PluginShell HostCapabilities");
  if (!capabilities_) {
    capabilities_ = std::make_unique<plugin::HostCapabilities>();
  }
  capabilities_->attach(host_);
  // Keep PainterRegistry out of content/: wire withdraw here only.
  host_->set_ui_withdraw_hook([](std::string_view id) {
    ui::views::PainterRegistry::get().withdraw_plugin(id);
  });
  if (pool_) {
    plugin::attach_host_processing(host_, pool_.get());
  }
  host_attached_ = true;
  return true;
}

void PluginShell::install_command_pack_enable() {
  detail::install_command_pack_enable(registry_.get());
}

void PluginShell::shutdown() {
  if (shutdown_done_) {
    return;
  }
  shutdown_done_ = true;
  detail::clear_command_pack_enable();
  plugin::clear_contribute_index();
  // Disable plugins before tearing host/registry. Guard against a half-inited
  // or already-freed registry (init failure → unique_ptr reset → ~PluginShell).
  if (registry_ && host_) {
    if (manager_) {
      manager_->destroy_all(host_);
    }
    const std::vector<plugin::PluginRecord> records = registry_->list();
    for (const plugin::PluginRecord& rec : records) {
      if (rec.state == plugin::PluginState::kEnabled) {
        registry_->set_enabled(rec.manifest.id, false, host_);
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
    capabilities_->detach(host_);
  }
  capabilities_.reset();
  // GisContents owns PluginHost — drop the non-owning view only.
  host_ = nullptr;
  host_attached_ = false;
  events_ = nullptr;
  registry_.reset();
  catalog_.reset();
  plugin::clear_resource_roots();
}

content::PluginHost* PluginShell::host() const {
  return host_;
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
  (void)ensure_command(processing_id);
  // Enqueue on the utility pool, then drain here so callers see completed
  // GisScene mutations before continuing (showcase export / marks).
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
  (void)ensure_command(command_id);
  if (host_->execute(command_id, args)) {
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
  python_->bind_host(host_);
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

bool PluginShell::ensure_discovered() {
  if (discovered_) {
    return true;
  }
  if (!registry_) {
    return false;
  }
  std::string root = plugins_dir_;
  if (root.empty()) {
    root = default_plugins_dir();
  }
  if (manager_) {
    (void)manager_->scan_directory(root);
  }
  discovered_ = true;
  return true;
}

bool PluginShell::ensure_command(std::string_view command_id) {
  if (!host_ || command_id.empty()) {
    return false;
  }
  (void)ensure_discovered();
  return plugin::ensure_for_command(host_, command_id);
}

bool PluginShell::start_builtins() {
  if (!registry_ || !host_) {
    return false;
  }
  (void)ensure_discovered();
  (void)app::register_builtin_plugins(registry_.get(), host_);
  // Builtins may add manifests after scan; refresh owner index.
  plugin::rebuild_contribute_index(*registry_);
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
  return detail::apply_registry_startup(registry_.get(), host_,
                                        &startup_viewport_, &startup_scenario_);
}

}  // namespace app

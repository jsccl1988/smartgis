// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/plugin_shell.h"

#include <cstdio>
#include <string>
#include <utility>
#include <windows.h>

#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/flood/commands.h"
#include "plugin/product/stormsurge/commands.h"
#include "plugin/product/geochem/commands.h"
#include "plugin/product/mine/commands.h"
#include "plugin/product/traffic/commands.h"
#include "plugin/runtime/host/ui/manager_view.h"
#include "plugin/runtime/host/manifest/manifest.h"
#include "plugin/product/print/commands.h"
#include "plugin/runtime/host/resources/resource_roots.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/host/processing/processing.h"
#include "plugin/runtime/host/registry/registry.h"
#include "plugin/runtime/python/runtime.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/paint/painter_registry.h"

namespace app {
namespace {

void stop_noop() {}

bool add_builtin(plugin::Registry* registry,
                 const char* id,
                 const char* name,
                 bool (*start)(content::PluginHost*)) {
  plugin::Manifest m;
  m.id = id;
  m.name = name;
  m.version = "1.0.0";
  m.api_version = 2;
  m.kind = plugin::PluginKind::kBuiltin;
  if (!registry->add_manifest(m, plugin::TrustClass::kBuiltin)) {
    return false;
  }
  return registry->register_builtin_hooks(id, start, stop_noop);
}

std::string default_plugins_dir() {
  char path[MAX_PATH] = {};
  // <exe_dir>/../plugins  (out/Debug → out/plugins)
  if (!detail::exe_sidecar_path_a(path, MAX_PATH, "..\\plugins")) {
    return {};
  }
  return std::string(path);
}

std::string join_plugins_package(const std::string& root,
                                 const char* package) {
  if (root.empty() || !package || !*package) {
    return {};
  }
  std::string out = root;
  const char last = out.back();
  if (last != '/' && last != '\\') {
    out.push_back('\\');
  }
  out.append(package);
  return out;
}

}  // namespace

PluginShell::PluginShell() = default;

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
  if (root.empty()) {
    return;
  }
  // Package folder names under --plugins-dir (not shared out/ui).
  plugin::set_resource_root("smartgis.world3d",
                            join_plugins_package(root, "world3d"));
  plugin::set_resource_root("smartgis.traffic",
                            join_plugins_package(root, "traffic"));
  plugin::set_resource_root("smartgis.flood",
                            join_plugins_package(root, "flood"));
  plugin::set_resource_root("smartgis.stormsurge",
                            join_plugins_package(root, "stormsurge"));
  plugin::set_resource_root("smartgis.mine",
                            join_plugins_package(root, "mine"));
  plugin::set_resource_root("smartgis.geochem",
                            join_plugins_package(root, "geochem"));
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
    catalog_ = std::make_unique<tool::CommandCatalog>();
    host_.reset(content::create_plugin_host(catalog_.get(), events, nullptr));
    if (!host_) {
      return false;
    }
  }
  // Keep PainterRegistry out of content/: wire withdraw here only.
  host_->set_ui_withdraw_hook([](std::string_view id) {
    ui::views::PainterRegistry::get().withdraw_plugin(id);
  });
  {
    BASE_TRACE_EVENT("PluginRegistry.setup", "startup");
    registry_ = std::make_unique<plugin::Registry>();
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
  host_.reset();
  registry_.reset();
  catalog_.reset();
  plugin::clear_resource_roots();
}

bool PluginShell::show_manager(HWND owner) {
  if (!registry_ || !host_) {
    return false;
  }
  (void)ensure_builtins();
  auto body =
      std::make_unique<plugin::ManagerView>(registry_.get(), host_.get());
  body->refresh();
  ui::views::Dialog::run_modal(owner, L"Plugins", 720, 420, std::move(body));
  return true;
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
  if (!host_ || command_id.empty()) {
    return false;
  }
  (void)ensure_builtins();
  return host_->execute(command_id, tool::CommandArgs{});
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

  struct Builtin {
    const char* id;
    const char* name;
    bool (*start)(content::PluginHost*);
  };
  // Display names match leftover AuxModule Ambox labels (UTF-8).
  const Builtin builtins[] = {
      {"smartgis.world3d", "DEM生成", plugin::register_world3d},
      {"smartgis.traffic", "城市交通最佳路径", plugin::register_traffic},
      {"smartgis.flood", "DEM洪水淹没", plugin::register_flood},
      {"smartgis.stormsurge", "风暴潮淹没", plugin::register_stormsurge},
      {"smartgis.mine", "矿山地层插值", plugin::register_mine},
      {"smartgis.geochem", "地球化学分析", plugin::register_geochem},
      {"smartgis.print", "地图打印", plugin::register_print},
      {"smartgis.processing", "Processing", plugin::register_builtin_processing},
  };

  for (const Builtin& b : builtins) {
    if (!add_builtin(registry_.get(), b.id, b.name, b.start)) {
      LOGGING(LOG_WARNING, "PluginShell: add_builtin failed id=%s", b.id);
      continue;
    }
    if (!registry_->set_enabled(b.id, true, host_.get())) {
      LOGGING(LOG_WARNING, "PluginShell: enable failed id=%s err=%s", b.id,
              registry_->last_error().c_str());
      continue;
    }
  }
  return true;
}

}  // namespace app

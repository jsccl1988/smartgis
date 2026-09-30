// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/plugin_shell.h"

#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/product/dem/commands.h"
#include "plugin/runtime/host/manager_view.h"
#include "plugin/runtime/host/manifest.h"
#include "plugin/product/model3d/commands.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/product/print/commands.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/host/processing.h"
#include "plugin/runtime/host/registry.h"
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

}  // namespace

PluginShell::PluginShell() = default;

PluginShell::~PluginShell() {
  shutdown();
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
  catalog_ = std::make_unique<tool::CommandCatalog>();
  host_.reset(content::create_plugin_host(catalog_.get(), events, nullptr));
  if (!host_) {
    return false;
  }
  // Keep PainterRegistry out of content/: wire withdraw here only.
  host_->set_ui_withdraw_hook([](std::string_view id) {
    ui::views::PainterRegistry::get().withdraw_plugin(id);
  });
  registry_ = std::make_unique<plugin::Registry>();
  pool_ = std::make_unique<plugin::ProcessingPool>(
      plugin::ProcessingMode::kThread);
  plugin::attach_host_processing(host_.get(), pool_.get());
  init_python();
  return start_builtins();
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
}

bool PluginShell::show_manager(HWND owner) {
  if (!registry_ || !host_) {
    return false;
  }
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
  return host_->run_processing(processing_id, args_json);
}

bool PluginShell::execute(std::string_view command_id) {
  if (!host_ || command_id.empty()) {
    return false;
  }
  return host_->execute(command_id, tool::CommandArgs{});
}

bool PluginShell::ensure_python() {
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
      {"smartgis.dem", "DEM生成", plugin::register_dem},
      {"smartgis.print", "地图打印", plugin::register_print},
      {"smartgis.model3d", "三维创建", plugin::register_model3d},
      {"smartgis.baogrid", "正交格网", plugin::register_orthogrid},
      {"smartgis.processing", "Processing", plugin::register_builtin_processing},
  };

  for (const Builtin& b : builtins) {
    if (!add_builtin(registry_.get(), b.id, b.name, b.start)) {
      return false;
    }
    if (!registry_->set_enabled(b.id, true, host_.get())) {
      return false;
    }
  }
  return true;
}

}  // namespace app

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/plugin_chrome.h"

#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/dem/dem_commands.h"
#include "plugin/host/manager_view.h"
#include "plugin/host/manifest.h"
#include "plugin/model3d/model3d_commands.h"
#include "plugin/orthogrid/commands.h"
#include "plugin/print/print_commands.h"
#include "plugin/host/processing.h"
#include "plugin/proj/proj_commands.h"
#include "plugin/host/registry.h"
#include "tool/command.h"
#include "ui/views/dialogs/dialog.h"

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

PluginChrome::PluginChrome() = default;

PluginChrome::~PluginChrome() {
  shutdown();
}

bool PluginChrome::init(content::EventBus* events) {
  catalog_ = std::make_unique<tool::CommandCatalog>();
  host_.reset(content::create_plugin_host(catalog_.get(), events, nullptr));
  if (!host_) {
    return false;
  }
  registry_ = std::make_unique<plugin::Registry>();
  pool_ = std::make_unique<plugin::ProcessingPool>(
      plugin::ProcessingMode::kThread);
  plugin::attach_host_processing(host_.get(), pool_.get());
  return start_builtins();
}

void PluginChrome::shutdown() {
  if (registry_ && host_) {
    for (const plugin::PluginRecord& rec : registry_->list()) {
      if (rec.state == plugin::PluginState::kEnabled) {
        registry_->set_enabled(rec.manifest.id, false, host_.get());
      }
    }
  }
  pool_.reset();
  host_.reset();
  registry_.reset();
  catalog_.reset();
}

bool PluginChrome::show_manager(HWND owner) {
  if (!registry_ || !host_) {
    return false;
  }
  auto body =
      std::make_unique<plugin::ManagerView>(registry_.get(), host_.get());
  body->refresh();
  ui::views::Dialog::run_modal(owner, L"Plugins", 720, 420, std::move(body));
  return true;
}

content::PluginHost* PluginChrome::host() const {
  return host_.get();
}

tool::CommandCatalog* PluginChrome::commands() const {
  return catalog_.get();
}

plugin::Registry* PluginChrome::registry() const {
  return registry_.get();
}

bool PluginChrome::execute(std::string_view command_id) {
  if (!host_ || command_id.empty()) {
    return false;
  }
  return host_->execute(command_id, tool::CommandArgs{});
}

bool PluginChrome::start_builtins() {
  if (!registry_ || !host_) {
    return false;
  }

  struct Builtin {
    const char* id;
    const char* name;
    bool (*start)(content::PluginHost*);
  };
  const Builtin builtins[] = {
      {"smartgis.dem", "DEM", plugin::register_dem},
      {"smartgis.proj", "Map Project", plugin::register_proj},
      {"smartgis.print", "Map Print", plugin::register_print},
      {"smartgis.model3d", "3D Model", plugin::register_model3d},
      {"smartgis.baogrid", "Orthogrid", plugin::register_orthogrid},
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

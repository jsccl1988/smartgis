// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/native/manager.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/contribute_index.h"
#include "plugin/runtime/host/catalog/resource_roots.h"

namespace plugin {
namespace {

std::string join_dll(const std::string& dir, const std::string& name) {
  if (name.empty()) {
    return dir;
  }
  if (name.find(':') != std::string::npos || name.find('\\') != std::string::npos ||
      name.find('/') != std::string::npos) {
    return name;
  }
  std::string path = dir;
  if (!path.empty()) {
    const char last = path.back();
    if (last != '\\' && last != '/') {
      path.push_back('\\');
    }
  }
  path += name;
  return path;
}

}  // namespace

PluginManager::PluginManager(Registry* registry) : registry_(registry) {
  if (!registry_) {
    return;
  }
  registry_->set_native_starter(
      [this](const PluginRecord& rec, content::PluginHost* host) {
        return start_native(rec, host);
      },
      [this](const PluginRecord& rec) { stop_native(rec); },
      [this](const PluginRecord& rec, std::string_view ev,
             std::string_view payload) { return run_native(rec, ev, payload); });
}

PluginManager::~PluginManager() = default;

PluginManager* plugin_manager_new(Registry* registry) {
  return new PluginManager(registry);
}

void plugin_manager_delete(PluginManager* manager) {
  delete manager;
}

int PluginManager::scan_directory(const std::string& plugins_dir) {
  last_error_.clear();
  discovered_ = scan_plugins_dir(plugins_dir, &last_error_);
  if (!registry_) {
    return static_cast<int>(discovered_.size());
  }
  int added = 0;
  for (const DiscoveredPlugin& d : discovered_) {
    if (d.manifest.id.empty()) {
      continue;
    }
    if (registry_->find(d.manifest.id)) {
      continue;
    }
    const TrustClass trust = d.manifest.kind == PluginKind::kBuiltin
                                 ? TrustClass::kBuiltin
                                 : TrustClass::kUnsignedTrusted;
    if (!registry_->add_manifest(d.manifest, trust)) {
      continue;
    }
    (void)registry_->set_directory(d.manifest.id, d.directory);
    if (!d.dll_path.empty()) {
      modules_[d.manifest.id] = std::make_unique<NativeModule>();
    }
    if (!d.directory.empty() && d.manifest.kind != PluginKind::kBuiltin) {
      set_resource_root(d.manifest.id, d.directory);
    }
    ++added;
  }
  // Manifest contributes → owner index (no LoadLibrary). Enables
  // ensure_for_command to resolve packs discovered on disk.
  if (registry_) {
    rebuild_contribute_index(*registry_);
  }
  return added;
}

bool PluginManager::init_all(content::PluginHost* host) {
  if (!registry_ || !host) {
    last_error_ = "no registry/host";
    return false;
  }
  bool any = false;
  for (const PluginRecord& rec : registry_->list()) {
    if (rec.manifest.kind != PluginKind::kNative) {
      continue;
    }
    if (rec.trust == TrustClass::kDenied) {
      continue;
    }
    if (registry_->set_enabled(rec.manifest.id, true, host)) {
      any = true;
    }
  }
  return any || !registry_->list().empty();
}

int PluginManager::dispatch_event(std::string_view event_id,
                                  std::string_view payload) {
  if (!registry_) {
    return 0;
  }
  int handled = 0;
  for (const PluginRecord& rec : registry_->list()) {
    if (rec.state != PluginState::kEnabled ||
        rec.manifest.kind != PluginKind::kNative) {
      continue;
    }
    if (run_native(rec, event_id, payload) > 0) {
      ++handled;
    }
  }
  return handled;
}

void PluginManager::destroy_all(content::PluginHost* host) {
  if (!registry_) {
    return;
  }
  for (const PluginRecord& rec : registry_->list()) {
    if (rec.state == PluginState::kEnabled) {
      registry_->set_enabled(rec.manifest.id, false, host);
    }
  }
  modules_.clear();
}

bool PluginManager::start_native(const PluginRecord& rec,
                                 content::PluginHost* host) {
  std::string dll_path;
  for (const DiscoveredPlugin& d : discovered_) {
    if (d.manifest.id == rec.manifest.id && !d.dll_path.empty()) {
      dll_path = d.dll_path;
      break;
    }
  }
  if (dll_path.empty()) {
    dll_path = join_dll(rec.directory, rec.manifest.library);
  }
  auto it = modules_.find(rec.manifest.id);
  if (it == modules_.end() || !it->second) {
    it = modules_.emplace(rec.manifest.id, std::make_unique<NativeModule>()).first;
  }
  NativeModule* mod = it->second.get();
  if (!mod->is_loaded() && !mod->load(dll_path)) {
    last_error_ = mod->last_error();
    return false;
  }
  if (mod->init(host) != 0) {
    last_error_ = mod->last_error();
    return false;
  }
  return true;
}

void PluginManager::stop_native(const PluginRecord& rec) {
  auto it = modules_.find(rec.manifest.id);
  if (it != modules_.end() && it->second) {
    it->second->destroy();
  }
}

int PluginManager::run_native(const PluginRecord& rec, std::string_view event_id,
                              std::string_view payload) {
  auto it = modules_.find(rec.manifest.id);
  if (it == modules_.end() || !it->second) {
    return 0;
  }
  return it->second->run(event_id, payload);
}

}  // namespace plugin

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_REGISTRY_H_
#define PLUGIN_REGISTRY_H_

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "plugin/runtime/host/catalog/manifest.h"
#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class PluginHost;
}

namespace plugin {

enum class PluginState {
  kDisabled,
  kEnabled,
  kError,
  kInvalidExports,
  kLoadFailed
};

enum class TrustClass {
  kBuiltin,
  kSignedOfficial,
  kUnsignedTrusted,
  kDenied
};

// One installed plugin as seen by Plugin Manager and enable/disable.
struct PluginRecord {
  Manifest manifest;
  PluginState state = PluginState::kDisabled;
  TrustClass trust = TrustClass::kDenied;
  std::string directory;
};

class PLUGIN_HOST_EXPORT Registry {
 public:
  Registry();

  bool add_manifest(const Manifest& m, TrustClass trust);
  // Build Manifest entirely inside plugin_host.dll. Do not construct a
  // Manifest with std::string in the EXE and pass it across the DLL boundary
  // (MSVC debug STL → _Xlength_error / heap corruption on assign).
  bool add_builtin_manifest(const char* id, const char* name);
  bool set_enabled(std::string_view id, bool enabled,
                   content::PluginHost* host);
  bool trust_unsigned(std::string_view id);
  bool unload(std::string_view id, content::PluginHost* host);
  const PluginRecord* find(std::string_view id) const;
  std::vector<PluginRecord> list() const;
  const std::string& last_error() const;
  bool register_builtin_hooks(std::string_view id,
                              bool (*start)(content::PluginHost*),
                              void (*stop)());

  void set_python_starter(
      std::function<bool(const PluginRecord&, content::PluginHost*)> start,
      std::function<void(const PluginRecord&)> stop);

  void set_native_starter(
      std::function<bool(const PluginRecord&, content::PluginHost*)> start,
      std::function<void(const PluginRecord&)> stop,
      std::function<int(const PluginRecord&, std::string_view, std::string_view)>
          run);

  bool set_directory(std::string_view id, std::string directory);

  void set_state_path(std::string path);
  bool load_state();
  bool save_state() const;

 private:
  PluginRecord* find_mut(std::string_view id);
  bool start_plugin(PluginRecord* rec, content::PluginHost* host);
  void stop_plugin(PluginRecord* rec, content::PluginHost* host);

  std::map<std::string, PluginRecord> records_;
  std::string last_error_;

  struct Hooks {
    bool (*start)(content::PluginHost*) = nullptr;
    void (*stop)() = nullptr;
  };
  std::map<std::string, Hooks> hooks_;

  std::function<bool(const PluginRecord&, content::PluginHost*)> python_start_;
  std::function<void(const PluginRecord&)> python_stop_;
  std::function<bool(const PluginRecord&, content::PluginHost*)> native_start_;
  std::function<void(const PluginRecord&)> native_stop_;
  std::function<int(const PluginRecord&, std::string_view, std::string_view)>
      native_run_;

  std::string state_path_;
  std::vector<std::string> trusted_unsigned_ids_;
  std::vector<std::string> enabled_ids_;
};

PLUGIN_HOST_EXPORT Registry* registry_new();
PLUGIN_HOST_EXPORT void registry_delete(Registry* r);

}  // namespace plugin

#endif  // PLUGIN_REGISTRY_H_

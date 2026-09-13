// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_REGISTRY_H_
#define PLUGIN_REGISTRY_H_

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "plugin/host/manifest.h"

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

class Registry {
 public:
  Registry();

  bool add_manifest(const Manifest& m, TrustClass trust);
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

  std::string state_path_;
  std::vector<std::string> trusted_unsigned_ids_;
  std::vector<std::string> enabled_ids_;
};

}  // namespace plugin

#endif  // PLUGIN_REGISTRY_H_

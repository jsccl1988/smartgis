// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_NATIVE_MODULE_H_
#define PLUGIN_RUNTIME_HOST_NATIVE_MODULE_H_

#include <string>
#include <string_view>

#include "base/util/library.h"
#include "plugin/runtime/host/native/exports.h"
#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class PluginHost;
}

namespace plugin {

// One loaded native DLL. Resolves init/run/destroy (AM start/stop fallbacks).
class PLUGIN_HOST_EXPORT NativeModule {
 public:
  NativeModule();
  ~NativeModule();

  NativeModule(const NativeModule&) = delete;
  NativeModule& operator=(const NativeModule&) = delete;

  bool load(const std::string& dll_path);
  int init(content::PluginHost* host);
  int run(std::string_view event_id, std::string_view payload);
  void destroy();
  void unload();
  bool is_loaded() const;
  const std::string& last_error() const;
  const std::string& path() const { return path_; }

 private:
  base::library lib_;
  std::string path_;
  std::string last_error_;
  PluginInitFn init_ = nullptr;
  PluginRunFn run_ = nullptr;
  PluginDestroyFn destroy_ = nullptr;
  void (*start_fallback_)() = nullptr;
  void (*stop_fallback_)() = nullptr;
  bool inited_ = false;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_NATIVE_MODULE_H_

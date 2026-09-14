// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_UTIL_PLUGIN_H_
#define BASE_UTIL_PLUGIN_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/util/library.h"
#include "base/util/path.h"

namespace base {

enum class plugin_state {
  unloaded,
  loaded,
  started,
  stopped,
  error,
};

// Generic dynlib plugin (version/start/stop). Product host lives in src/plugin.
class plugin : public library {
 public:
  using fn_version = int();
  using fn_start = void();
  using fn_stop = void();

  plugin(std::string name, path filename)
      : library(std::move(filename)), name_(std::move(name)) {}

  ~plugin() {
    if (state_ == plugin_state::started) {
      stop();
    }
    if (state_ != plugin_state::unloaded) {
      unload();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(plugin);

  const std::string& name() const { return name_; }
  plugin_state state() const { return state_; }
  int version() const { return version_number_; }

  bool load() {
    if (state_ != plugin_state::unloaded && state_ != plugin_state::error) {
      return false;
    }
    if (!library::load()) {
      state_ = plugin_state::error;
      return false;
    }
    // Prefer SmartGIS legacy exports when present.
    version_ = resolve<fn_version>("get_plugin_version");
    start_ = resolve<fn_start>("start_plugin");
    stop_ = resolve<fn_stop>("stop_plugin");
    if (!version_) {
      version_ = resolve<fn_version>("version");
    }
    if (!start_) {
      start_ = resolve<fn_start>("start");
    }
    if (!stop_) {
      stop_ = resolve<fn_stop>("stop");
    }
    if (!version_ || !start_ || !stop_) {
      LOGGING(LOG_ERROR, "Plugin %s missing required exports", name_.c_str());
      library::unload();
      state_ = plugin_state::error;
      return false;
    }
    version_number_ = version_();
    state_ = plugin_state::loaded;
    return true;
  }

  bool start() {
    if (state_ != plugin_state::loaded && state_ != plugin_state::stopped) {
      return false;
    }
    start_();
    state_ = plugin_state::started;
    return true;
  }

  bool stop() {
    if (state_ != plugin_state::started) {
      return false;
    }
    stop_();
    state_ = plugin_state::stopped;
    return true;
  }

  bool unload() {
    if (state_ == plugin_state::started) {
      stop();
    }
    bool ok = library::unload();
    state_ = plugin_state::unloaded;
    version_ = nullptr;
    start_ = nullptr;
    stop_ = nullptr;
    return ok;
  }

 private:
  std::string name_;
  fn_version* version_ = nullptr;
  fn_start* start_ = nullptr;
  fn_stop* stop_ = nullptr;
  plugin_state state_ = plugin_state::unloaded;
  int version_number_ = 0;
};

// Simple registry; product hosts may wrap this (see src/plugin).
class plugin_manager {
 public:
  static plugin_manager& instance() {
    static plugin_manager mgr;
    return mgr;
  }

  plugin* load_plugin(const std::string& name, const path& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    if (it != plugins_.end()) {
      return it->second.get();
    }
    auto p = std::make_unique<plugin>(name, filename);
    if (!p->load()) {
      return nullptr;
    }
    plugin* raw = p.get();
    plugins_.emplace(name, std::move(p));
    return raw;
  }

  void unload_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : plugins_) {
      kv.second->unload();
    }
    plugins_.clear();
  }

 private:
  plugin_manager() = default;
  std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<plugin>> plugins_;
};

}  // namespace base

#endif  // BASE_UTIL_PLUGIN_H_

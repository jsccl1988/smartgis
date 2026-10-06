// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/native/module.h"

#include "base/util/path.h"

namespace plugin {

NativeModule::NativeModule() = default;

NativeModule::~NativeModule() {
  destroy();
  unload();
}

bool NativeModule::load(const std::string& dll_path) {
  last_error_.clear();
  unload();
  path_ = dll_path;
  if (dll_path.empty()) {
    last_error_ = "empty dll path";
    return false;
  }
  if (!lib_.load(base::path(dll_path))) {
    last_error_ = lib_.last_error().empty() ? "LoadLibrary failed" : lib_.last_error();
    return false;
  }
  init_ = lib_.resolve<int(void*)>(kExportInit);
  run_ = lib_.resolve<int(const char*, const char*)>(kExportRun);
  destroy_ = lib_.resolve<void()>(kExportDestroy);
  start_fallback_ = lib_.resolve<void()>("start_plugin");
  if (!start_fallback_) {
    start_fallback_ = lib_.resolve<void()>("StartPlugin");
  }
  if (!start_fallback_) {
    start_fallback_ = lib_.resolve<void()>("start");
  }
  stop_fallback_ = lib_.resolve<void()>("stop_plugin");
  if (!stop_fallback_) {
    stop_fallback_ = lib_.resolve<void()>("StopPlugin");
  }
  if (!stop_fallback_) {
    stop_fallback_ = lib_.resolve<void()>("stop");
  }
  if (!init_ && !start_fallback_) {
    last_error_ = "missing init/start export";
    lib_.unload();
    return false;
  }
  if (!destroy_ && !stop_fallback_) {
    last_error_ = "missing destroy/stop export";
    lib_.unload();
    return false;
  }
  return true;
}

int NativeModule::init(content::PluginHost* host) {
  if (!lib_.is_loaded()) {
    last_error_ = "not loaded";
    return -1;
  }
  if (inited_) {
    return 0;
  }
  if (init_) {
    const int rc = init_(static_cast<void*>(host));
    if (rc != 0) {
      last_error_ = "init failed";
      return rc;
    }
  } else if (start_fallback_) {
    start_fallback_();
  }
  inited_ = true;
  return 0;
}

int NativeModule::run(std::string_view event_id, std::string_view payload) {
  if (!inited_ || !run_) {
    return 0;
  }
  const std::string ev(event_id);
  const std::string pay(payload);
  return run_(ev.c_str(), pay.c_str());
}

void NativeModule::destroy() {
  if (!inited_) {
    return;
  }
  if (destroy_) {
    destroy_();
  } else if (stop_fallback_) {
    stop_fallback_();
  }
  inited_ = false;
}

void NativeModule::unload() {
  destroy();
  init_ = nullptr;
  run_ = nullptr;
  destroy_ = nullptr;
  start_fallback_ = nullptr;
  stop_fallback_ = nullptr;
  if (lib_.is_loaded()) {
    lib_.unload();
  }
}

bool NativeModule::is_loaded() const {
  return lib_.is_loaded();
}

const std::string& NativeModule::last_error() const {
  return last_error_;
}

}  // namespace plugin

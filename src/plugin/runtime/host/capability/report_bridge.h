// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_REPORT_BRIDGE_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_REPORT_BRIDGE_H_

#include <functional>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Shell-installed local HTML report dock. Registered as kCapabilityReport.
class ReportBridge {
 public:
  using OpenFn = std::function<bool(std::string_view report_dir)>;
  using PostFn = std::function<bool(std::string_view json)>;
  using CloseFn = std::function<void()>;

  PLUGIN_HOST_EXPORT void set_bridges(OpenFn open, PostFn post, CloseFn close);

  bool open(std::string_view report_dir) const {
    return open_ ? open_(report_dir) : false;
  }
  bool post(std::string_view json) const {
    return post_ ? post_(json) : false;
  }
  void close() const {
    if (close_) {
      close_();
    }
  }

 private:
  OpenFn open_;
  PostFn post_;
  CloseFn close_;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_REPORT_BRIDGE_H_

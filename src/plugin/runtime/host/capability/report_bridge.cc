// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/capability/report_bridge.h"

namespace plugin {

void ReportBridge::set_bridges(OpenFn open, PostFn post, CloseFn close) {
  open_ = std::move(open);
  post_ = std::move(post);
  close_ = std::move(close);
}

}  // namespace plugin

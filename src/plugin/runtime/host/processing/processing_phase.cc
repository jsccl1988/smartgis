// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/processing/processing_phase.h"

namespace plugin {

bool run_processing_phases(content::PluginHost* host,
                           std::string_view args_json,
                           const ProcessingComputeFn& compute,
                           const ProcessingPresentFn& present) {
  if (host) {
    return present ? present(host) : false;
  }
  return compute ? compute(args_json) : false;
}

}  // namespace plugin

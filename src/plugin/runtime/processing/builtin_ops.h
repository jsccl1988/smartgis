// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PROCESSING_BUILTIN_OPS_H_
#define PLUGIN_PROCESSING_BUILTIN_OPS_H_

#include "plugin/runtime/processing/ops_runner.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Register every catalog entry on |host| under plugin id smartgis.processing.
bool register_builtin_processing(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_PROCESSING_BUILTIN_OPS_H_

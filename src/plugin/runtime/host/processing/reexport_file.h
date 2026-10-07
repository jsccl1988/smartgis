// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_PROCESSING_REEXPORT_FILE_H_
#define PLUGIN_RUNTIME_HOST_PROCESSING_REEXPORT_FILE_H_

#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Shared "export last artifact" path used by flood / stormsurge / traffic:
// read optional args.output; copy cached → dest when different; else verify
// cached exists. Updates *cached_path on successful copy. Sets operation_result.
PLUGIN_HOST_EXPORT bool reexport_cached_file(std::string* cached_path,
                                             std::string_view args_json,
                                             const char* op_name);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_PROCESSING_REEXPORT_FILE_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_OPERATION_RESULT_H_
#define PLUGIN_RUNTIME_HOST_OPERATION_RESULT_H_

#include <string>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Last structured result from a product command or processing factory.
// ProcessingPool copies this into the host done(bool, string) callback.
PLUGIN_HOST_EXPORT void set_operation_result(std::string message);
PLUGIN_HOST_EXPORT std::string operation_result();

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_OPERATION_RESULT_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_RESOURCE_ROOTS_H_
#define PLUGIN_RUNTIME_HOST_RESOURCE_ROOTS_H_

#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Per-plugin resource directory (absolute or process-cwd-relative).
// Product plugins load markup / assets from here — not from shared out/ui.
// Shell sets roots at startup (--plugins-dir=<root> → <root>/<plugin_id>/).
PLUGIN_HOST_EXPORT void set_resource_root(std::string_view plugin_id,
                                          std::string directory);
PLUGIN_HOST_EXPORT void clear_resource_roots();

// Returns the directory previously set for |plugin_id|, or empty.
PLUGIN_HOST_EXPORT std::string resource_root(std::string_view plugin_id);

// Joins resource_root(plugin_id) + relative path. Empty if root unset.
PLUGIN_HOST_EXPORT std::string resolve_resource(std::string_view plugin_id,
                                                std::string_view relative);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_RESOURCE_ROOTS_H_

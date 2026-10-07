// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CATALOG_CONTRIBUTE_INDEX_H_
#define PLUGIN_RUNTIME_HOST_CATALOG_CONTRIBUTE_INDEX_H_

#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

struct Manifest;
class Registry;

// Manifest contributes → owning plugin id, built after scan (no LoadLibrary).
// Exact command/processing ids plus first-segment stems (e.g. "map2d", "print").

PLUGIN_HOST_EXPORT void clear_contribute_index();
PLUGIN_HOST_EXPORT void index_manifest_contributes(const Manifest& manifest);
PLUGIN_HOST_EXPORT void rebuild_contribute_index(const Registry& registry);

// Exact id, else longest stem prefix of |id|. Writes owning plugin id.
PLUGIN_HOST_EXPORT bool lookup_contribute_owner(std::string_view id,
                                                std::string* out_plugin_id);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CATALOG_CONTRIBUTE_INDEX_H_

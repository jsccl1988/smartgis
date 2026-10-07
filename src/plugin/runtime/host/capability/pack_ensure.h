// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_PACK_ENSURE_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_PACK_ENSURE_H_

#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Ensure order for a command / processing id:
//  1) already in CommandCatalog
//  2) in-process PackEnsureFn list (longest matching prefix; harness scenario)
//  3) manifest contribute_index → enable plugin (scan, no LoadLibrary-all)
//
// Product DLLs contribute via native init after enable; harness TUs may
// register_command_pack for scenario bodies not in the DLL.

using PackEnsureFn = bool (*)(content::PluginHost* host);
using PackEnableFn = bool (*)(content::PluginHost* host,
                              std::string_view plugin_id);

PLUGIN_HOST_EXPORT void register_command_pack(std::string_view prefix,
                                              PackEnsureFn ensure);

PLUGIN_HOST_EXPORT void set_command_pack_enable(PackEnableFn enable);

PLUGIN_HOST_EXPORT bool ensure_for_command(content::PluginHost* host,
                                           std::string_view command_id);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_PACK_ENSURE_H_

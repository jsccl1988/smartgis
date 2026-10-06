// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

namespace plugin {
bool register_world3d(content::PluginHost* host);
}

#define PLUGIN_PACK_ID "smartgis.world3d"
#define PLUGIN_PACK_REGISTER(host) plugin::register_world3d(host)
#include "plugin/product/native_entry.h"

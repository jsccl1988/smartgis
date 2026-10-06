// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

namespace plugin {
bool register_map2d(content::PluginHost* host);
}

#define PLUGIN_PACK_ID "smartgis.map2d"
#define PLUGIN_PACK_REGISTER(host) plugin::register_map2d(host)
#include "plugin/product/native_entry.h"

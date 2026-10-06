// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

namespace plugin {
bool register_traffic(content::PluginHost* host);
}

#define PLUGIN_PACK_ID "smartgis.traffic"
#define PLUGIN_PACK_REGISTER(host) plugin::register_traffic(host)
#include "plugin/product/native_entry.h"

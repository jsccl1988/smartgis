// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

namespace plugin {
bool register_mine(content::PluginHost* host);
}

#define PLUGIN_PACK_ID "smartgis.mine"
#define PLUGIN_PACK_REGISTER(host) plugin::register_mine(host)
#include "plugin/product/native_entry.h"

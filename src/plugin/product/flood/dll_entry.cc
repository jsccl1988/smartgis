// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

namespace plugin {
bool register_flood(content::PluginHost* host);
}

#define PLUGIN_PACK_ID "smartgis.flood"
#define PLUGIN_PACK_REGISTER(host) plugin::register_flood(host)
#include "plugin/product/native_entry.h"

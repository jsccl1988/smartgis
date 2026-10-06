// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/plugin_host.h"

namespace plugin {
bool register_geochem(content::PluginHost* host);
}

#define PLUGIN_PACK_ID "smartgis.geochem"
#define PLUGIN_PACK_REGISTER(host) plugin::register_geochem(host)
#include "plugin/product/native_entry.h"

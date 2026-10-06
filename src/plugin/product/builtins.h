// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_BUILTINS_H_
#define PLUGIN_PRODUCT_BUILTINS_H_

#include <string>

namespace content {
class PluginHost;
}

namespace plugin {

class Registry;

// Horizon PluginShell must not include product commands.h. This façade owns the
// shipped builtin table (product packs + smartgis.processing) and resource
// roots under --plugins-dir.
bool register_builtin_plugins(Registry* registry, content::PluginHost* host);

// Maps each product plugin id → <plugins_dir>/<package> for samples / styles.
void install_product_resource_roots(const std::string& plugins_dir);

}  // namespace plugin

#endif  // PLUGIN_PRODUCT_BUILTINS_H_

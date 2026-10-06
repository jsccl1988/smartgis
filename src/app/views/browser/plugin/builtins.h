// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_PLUGIN_BUILTINS_H_
#define APP_VIEWS_PLUGIN_BUILTINS_H_

#include <cstddef>
#include <string>

namespace content {
class PluginHost;
}

namespace plugin {
class Registry;
}

namespace app {

// One shipped builtin pack. |start| is the plugin registrar. String pointers
// must outlive the process (use literals). |resource_package| is the folder
// name under --plugins-dir; null/empty skips set_resource_root.
struct BuiltinPlugin {
  const char* id = nullptr;
  const char* name = nullptr;
  bool (*start)(content::PluginHost*) = nullptr;
  const char* resource_package = nullptr;
};

// Called from each plugin TU (static initializer) so PluginShell never names
// product packs. Duplicate ids are ignored. Safe before main().
bool append_builtin_plugin(const BuiltinPlugin& spec);

// Snapshot of packs appended so far. |count| may be null.
const BuiltinPlugin* builtin_plugins(std::size_t* count);

// PluginShell must not include product commands.h. Enables every appended
// pack on |registry| / |host|.
bool register_builtin_plugins(plugin::Registry* registry,
                              content::PluginHost* host);

// Maps each appended |resource_package| → <plugins_dir>/<package>.
void install_builtin_resource_roots(const std::string& plugins_dir);

}  // namespace app

#endif  // APP_VIEWS_PLUGIN_BUILTINS_H_

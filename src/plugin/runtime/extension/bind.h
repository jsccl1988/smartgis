// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Wires Registry python start/stop to glang.ext mount.
// PythonRuntime lives in this directory. Host types live on glang.plugin.

#ifndef PLUGIN_RUNTIME_EXTENSION_BIND_H_
#define PLUGIN_RUNTIME_EXTENSION_BIND_H_

namespace plugin {

class Registry;
class PythonRuntime;

// Mounts glang.ext.<plugin_id> before PythonRuntime::start and unmounts
// after stop. plugin_id is Manifest.id.
void bind_registry_python_ext(Registry* registry, PythonRuntime* runtime);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_EXTENSION_BIND_H_

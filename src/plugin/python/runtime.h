// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PYTHON_RUNTIME_H_
#define PLUGIN_PYTHON_RUNTIME_H_

#include <string>
#include <string_view>

namespace content {
class PluginHost;
}

namespace plugin {

class Registry;

// In-process CPython 3.12 embed. init() is false when the interpreter
// cannot start (no DLL / headers compiled out).
class PythonRuntime {
 public:
  bool init();
  void shutdown();
  bool start(std::string_view directory, std::string_view entry,
             content::PluginHost* host);
  void stop();
  bool is_ready() const;

 private:
  bool ready_ = false;
};

void register_smartgis_bindings();
void bind_registry_python(Registry* registry, PythonRuntime* runtime);

}  // namespace plugin

#endif  // PLUGIN_PYTHON_RUNTIME_H_

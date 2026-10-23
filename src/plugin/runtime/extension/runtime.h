// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// kind=python lifecycle owned by plugin/runtime/extension.
// Host / ui / gis types are attached to glang.plugin at init.

#ifndef PLUGIN_RUNTIME_EXTENSION_RUNTIME_H_
#define PLUGIN_RUNTIME_EXTENSION_RUNTIME_H_

#include <string>
#include <string_view>

namespace content {
class PluginHost;
}

namespace plugin {

// In-process CPython 3.12 embed. init() is false when the interpreter
// cannot start (no DLL / headers compiled out).
class PythonRuntime {
 public:
  bool init();
  void shutdown();
  bool start(std::string_view directory, std::string_view entry,
             content::PluginHost* host);
  // Unload every kind=python module. Prefer stop(directory) from Registry.
  void stop();
  void stop(std::string_view directory);
  bool is_ready() const;

  // Evaluate |code| in __main__ (capture stdout + expression repr).
  // Returns output text; never throws.
  std::string eval(std::string_view code);

  // Execute |code| as statements in __main__. False when the interpreter
  // raises. |error| receives the exception text. String eval stays on eval();
  // a .py path must use exec_file() so main() runs.
  bool exec_source(std::string_view code, std::string* error);

  // Load |path| as a named module on the isolated interpreter, call main(),
  // then drop that module from sys.modules. False when main is missing or
  // the call raises. Does not detach live views.
  bool exec_file(std::string_view path, std::string* error);

  // Bind PluginHost into smartgis.content.host (idempotent).
  void bind_host(content::PluginHost* host);

 private:
  bool ready_ = false;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_EXTENSION_RUNTIME_H_

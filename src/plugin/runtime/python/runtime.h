// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PYTHON_RUNTIME_H_
#define PLUGIN_PYTHON_RUNTIME_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace content {
class PluginHost;
}

namespace plugin {

class Registry;

// Map-document + horizon hooks for Console / plugins (no GIS headers in Agent).
struct GisConsoleBridge {
  std::function<bool(const std::string& path)> write_active_geojson;
  std::function<bool(const std::string& path)> load_result_geojson;
  std::function<int()> feature_count;
  std::function<void()> refresh_gis;
  std::function<void()> flush_processing;

  // Scene (GisScene façade) — string / bool only.
  std::function<std::string()> layers_json;
  std::function<bool(const std::string& id)> select_layer;
  std::function<bool(const std::string& id, bool visible)> set_layer_visible;
  std::function<std::string()> extent_json;
  std::function<bool(const std::string& path)> open_path;
  std::function<bool(const std::string& path)> write_path;
  std::function<std::string()> present_mode;
  std::function<bool(const std::string& mode)> set_present_mode;

  // Cartographic StyleDocument on the active GisScene.
  std::function<bool()> has_style_document;
  std::function<bool(const std::string& path)> load_style_path;
  std::function<void()> clear_style;
  std::function<std::string()> style_summary_json;

  // Interactive tool activation (GisContents::ActivateTool). Unset → False.
  std::function<bool(uint32_t view_id, const std::string& tool_id)> activate_tool;
};

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

  // Bind PluginHost into smartgis.content.host (idempotent).
  void bind_host(content::PluginHost* host);

 private:
  bool ready_ = false;
};

void register_smartgis_bindings();
void bind_registry_python(Registry* registry, PythonRuntime* runtime);

void set_gis_console_bridge(GisConsoleBridge bridge);
void clear_gis_console_bridge();
void bind_gis_host_for_analysis(content::PluginHost* host);

// Invokes GisConsoleBridge::activate_tool when bound; otherwise false.
bool try_activate_tool(uint32_t view_id, const std::string& tool_id);

}  // namespace plugin

#endif  // PLUGIN_PYTHON_RUNTIME_H_

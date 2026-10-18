// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Map-document hooks the shell fills so GIS verbs do not include the chrome
// document type. Python only marshals; the temp-file analysis policy lives
// here.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_BIND_GIS_CONSOLE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_BIND_GIS_CONSOLE_H_

#include "content/browser/capability/op.h"
#include "content/browser/glang.runtime/exec/extend/exec.h"

#include <concepts>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace content {
namespace detail {

// A console hook is an empty-checkable callable. Return type selects the call.
template <typename F, typename... Args>
concept console_hook = std::invocable<F, Args...> && requires(const F& fn) {
  static_cast<bool>(fn);
};

template <typename F, typename R, typename... Args>
concept console_hook_returning =
    console_hook<F, Args...> &&
    std::same_as<std::invoke_result_t<const F&, Args...>, R>;

template <typename F, typename... Args>
  requires console_hook_returning<F, void, Args...>
void call_hook(const F& fn, Args&&... args) {
  if (fn) {
    std::invoke(fn, std::forward<Args>(args)...);
  }
}

template <typename F, typename... Args>
  requires console_hook_returning<F, bool, Args...>
bool call_hook(const F& fn, Args&&... args) {
  return static_cast<bool>(fn) && std::invoke(fn, std::forward<Args>(args)...);
}

template <typename F, typename R, typename... Args>
  requires console_hook_returning<F, R, Args...> &&
           (!std::same_as<R, bool>) && (!std::same_as<R, void>)
R call_hook(const F& fn, R fallback, Args&&... args) {
  if (!fn) {
    return fallback;
  }
  return std::invoke(fn, std::forward<Args>(args)...);
}

// First bound bool hook wins. Used when write_path falls back to write_active.
template <typename F, typename... Args>
  requires console_hook_returning<F, bool, Args...>
bool call_first_hook(const F& primary, const F& fallback, Args&&... args) {
  if (primary) {
    return std::invoke(primary, std::forward<Args>(args)...);
  }
  return call_hook(fallback, std::forward<Args>(args)...);
}

}  // namespace detail

class PluginHost;

// Map-document hooks for Console scripts. Each slot is detail::Op, so
// assignment accepts only an op_callable of that signature. Empty is false.
struct GisConsoleBridge {
  detail::Op<bool(const std::string& path)> write_active_geojson;
  detail::Op<bool(const std::string& path)> load_result_geojson;
  detail::Op<int()> feature_count;
  detail::Op<void()> refresh_gis;
  detail::Op<void()> flush_processing;

  detail::Op<std::string()> layers_json;
  detail::Op<bool(const std::string& id)> select_layer;
  detail::Op<bool(const std::string& id, bool visible)> set_layer_visible;
  detail::Op<std::string()> extent_json;
  detail::Op<bool(const std::string& path)> open_path;
  detail::Op<bool(const std::string& path)> write_path;
  detail::Op<std::string()> present_mode;
  detail::Op<bool(const std::string& mode)> set_present_mode;

  detail::Op<bool()> has_style_document;
  detail::Op<bool(const std::string& path)> load_style_path;
  detail::Op<void()> clear_style;
  detail::Op<std::string()> style_summary_json;

  // Interactive tool activation. Unset → try_activate_tool returns false.
  detail::Op<bool(uint32_t view_id, const std::string& tool_id)> activate_tool;
};

void set_gis_console_bridge(GisConsoleBridge bridge);
void clear_gis_console_bridge();
const GisConsoleBridge& gis_console_bridge();

void bind_gis_host_for_analysis(PluginHost* host);

// Invokes GisConsoleBridge::activate_tool when bound; otherwise false.
bool try_activate_tool(uint32_t view_id, const std::string& tool_id);

}  // namespace content

namespace content {
namespace code {

// One GIS verb against the console host. |error| is set when eval fails.
struct ConsoleText {
  bool ok = false;
  std::string text;
  std::string error;
};

ConsoleText console_text(std::string_view op,
                         const GisArg* args,
                         int count,
                         bool refresh);

// Extra kwargs are already JSON literals (Python scalar marshalling).
struct MapAnalysisRequest {
  std::string id;
  double distance = 0.05;
  std::string input;
  std::string output;
  std::vector<std::pair<std::string, std::string>> extra_json;
};

// |error| set → caller raises. Otherwise return the dict fields.
struct MapAnalysisResult {
  std::string error;
  bool ok = false;
  std::string text;
  int feature_count = 0;
};

MapAnalysisResult run_map_analysis(const MapAnalysisRequest& request);

}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_BIND_GIS_CONSOLE_H_

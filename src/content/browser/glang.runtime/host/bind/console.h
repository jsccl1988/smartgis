// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Console / map-analysis runners. Hooks live on CapabilityHost document/view
// lanes. Python only marshals; temp-file analysis policy lives here.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_CONSOLE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_CONSOLE_H_

#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/vm/exec.h"

#include <concepts>
#include <cstdint>
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

// First bound bool hook wins (e.g. write_document falls back to write_active).
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

// Installs console document/view Ops used by analysis and :py GIS.
void set_console_host(CapabilityHost host);
void clear_console_host();
const CapabilityHost& console_host();

// Binds PluginHost for run_processing inside map analysis.
void bind_console_plugin(PluginHost* host);

// Invokes view.activate_tool_at when bound; otherwise false.
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

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_BIND_CONSOLE_H_

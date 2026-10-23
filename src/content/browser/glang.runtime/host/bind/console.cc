// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/bind/console.h"

#include "content/browser/capability/host.h"
#include "content/public/plugin_host.h"

#include <filesystem>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

content::CapabilityHost g_console;
content::PluginHost* g_host = nullptr;

std::string json_escape_path(std::string_view path) {
  std::string out;
  out.reserve(path.size());
  for (char c : path) {
    if (c == '\\') {
      out += '/';
    } else if (c == '"') {
      out += "\\\"";
    } else {
      out += c;
    }
  }
  return out;
}

// Compile-time analysis op. The runtime id picks which extra JSON fields to emit.
template <typename Op>
concept analysis_op = requires {
  { Op::id } -> std::convertible_to<std::string_view>;
  { Op::needs_distance } -> std::convertible_to<bool>;
  { Op::needs_clip } -> std::convertible_to<bool>;
};

struct BufferOp {
  static constexpr std::string_view id = "native.buffer";
  static constexpr bool needs_distance = true;
  static constexpr bool needs_clip = false;
};
struct SimplifyOp {
  static constexpr std::string_view id = "native.simplify";
  static constexpr bool needs_distance = true;
  static constexpr bool needs_clip = false;
};
struct ClipOp {
  static constexpr std::string_view id = "native.clip";
  static constexpr bool needs_distance = false;
  static constexpr bool needs_clip = true;
};
struct IntersectionOp {
  static constexpr std::string_view id = "native.intersection";
  static constexpr bool needs_distance = false;
  static constexpr bool needs_clip = true;
};
struct UnionOp {
  static constexpr std::string_view id = "native.union";
  static constexpr bool needs_distance = false;
  static constexpr bool needs_clip = true;
};
struct DifferenceOp {
  static constexpr std::string_view id = "native.difference";
  static constexpr bool needs_distance = false;
  static constexpr bool needs_clip = true;
};
struct SymmetricDifferenceOp {
  static constexpr std::string_view id = "native.symmetric_difference";
  static constexpr bool needs_distance = false;
  static constexpr bool needs_clip = true;
};

template <analysis_op Op>
void append_op_fields(std::ostream& args,
                      const content::code::MapAnalysisRequest& request,
                      const std::filesystem::path& in) {
  if constexpr (Op::needs_distance) {
    args << ",\"distance\":" << request.distance;
  }
  if constexpr (Op::needs_clip) {
    args << ",\"clip\":\"" << json_escape_path(in.string()) << "\"";
  }
}

template <analysis_op... Ops>
void append_matched_op(std::ostream& args,
                       std::string_view id,
                       const content::code::MapAnalysisRequest& request,
                       const std::filesystem::path& in) {
  bool matched = false;
  auto one = [&](auto op) {
    using Op = decltype(op);
    if (matched || Op::id != id) {
      return;
    }
    matched = true;
    append_op_fields<Op>(args, request, in);
  };
  (one(Ops{}), ...);
}

// Overlay PluginHost processing onto the console CapabilityHost copy.
content::CapabilityHost analysis_host() {
  content::CapabilityHost host = g_console;
  host.plugin.run_processing = [](const std::string& id,
                                   const std::string& args) {
    return g_host && g_host->run_processing(id, args);
  };
  return host;
}

}  // namespace

namespace content {

void set_console_host(CapabilityHost host) {
  g_console = std::move(host);
}

void clear_console_host() {
  g_console = {};
}

const CapabilityHost& console_host() {
  return g_console;
}

void bind_console_plugin(PluginHost* host) {
  g_host = host;
}

bool try_activate_tool(uint32_t view_id, const std::string& tool_id) {
  return detail::call_hook(g_console.view.activate_tool_at, view_id, tool_id);
}

}  // namespace content

namespace content {
namespace code {

ConsoleText console_text(std::string_view op,
                         const GisArg* args,
                         int count,
                         bool refresh) {
  ConsoleText out;
  content::CapabilityHost host = analysis_host();
  if (!eval_gis_call(host, op, args, count, &out.text, &out.error)) {
    if (out.error.empty()) {
      out.error = std::string(op);
    }
    return out;
  }
  out.ok = true;
  if (refresh && out.text != "0") {
    content::detail::call_hook(g_console.document.refresh_gis);
  }
  return out;
}

MapAnalysisResult run_map_analysis(const MapAnalysisRequest& request) {
  MapAnalysisResult result;
  if (!g_host) {
    result.error = "PluginHost not bound";
    return result;
  }

  const bool explicit_io = !request.input.empty() && !request.output.empty();
  if (!explicit_io && (!g_console.document.write_active_geojson ||
                       !g_console.document.load_result_geojson)) {
    result.error = "gis console host not bound";
    return result;
  }

  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path();
  const fs::path in = explicit_io ? fs::path(request.input)
                                  : dir / "smartgis_py_analysis_in.geojson";
  const fs::path out = explicit_io ? fs::path(request.output)
                                   : dir / "smartgis_py_analysis_out.geojson";

  if (!explicit_io && !g_console.document.write_active_geojson(in.string())) {
    result.error = "write_active_geojson failed";
    return result;
  }

  std::ostringstream args_json;
  args_json << "{\"input\":\"" << json_escape_path(in.string())
            << "\",\"output\":\"" << json_escape_path(out.string()) << "\"";
  append_matched_op<BufferOp, SimplifyOp, ClipOp, IntersectionOp, UnionOp,
                    DifferenceOp, SymmetricDifferenceOp>(
      args_json, request.id, request, in);
  for (const auto& extra : request.extra_json) {
    args_json << ",\"" << json_escape_path(extra.first) << "\":" << extra.second;
  }
  args_json << "}";

  GisArg call_args[2] = {};
  call_args[0].text = request.id;
  call_args[0].kind = ArgKind::kStr;
  call_args[1].text = args_json.str();
  call_args[1].kind = ArgKind::kStr;

  std::string ignored;
  std::string il_error;
  content::CapabilityHost host = analysis_host();
  const bool ok = eval_gis_call(host, "run_processing", call_args, 2, &ignored,
                                &il_error);
  content::detail::call_hook(g_console.document.flush_processing);
  result.ok = ok;
  if (!ok) {
    result.text = "Processing failed: " + request.id;
    if (!il_error.empty()) {
      result.text += " (";
      result.text += il_error;
      result.text += ")";
    }
    return result;
  }
  if (!explicit_io && !g_console.document.load_result_geojson(out.string())) {
    result.ok = false;
    result.text = "write-back failed: " + request.id;
    return result;
  }
  if (g_console.document.feature_count) {
    result.feature_count = g_console.document.feature_count();
  }
  result.text = "ok: " + request.id +
                " features=" + std::to_string(result.feature_count);
  content::detail::call_hook(g_console.document.refresh_gis);
  return result;
}

}  // namespace code
}  // namespace content

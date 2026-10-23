// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/install.h"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>


#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace content {
namespace code {
namespace {
void mark(content::CapabilityHost& host, std::string_view token) {
  if (host.horizon.mark) {
    host.horizon.mark(std::string(token));
  }
}

bool run_processing(content::CapabilityHost& host,
                           std::string_view id,
                           std::string_view args_json) {
  return !id.empty() && host.plugin.run_processing &&
         host.plugin.run_processing(std::string(id), std::string(args_json));
}

bool run_plugin_command(content::CapabilityHost& host,
                               std::string_view id) {
  return !id.empty() && host.plugin.run_plugin_command &&
         host.plugin.run_plugin_command(std::string(id));
}

bool resolve_data(content::CapabilityHost& host,
                         std::string_view kind,
                         std::string_view leaf,
                         std::string* out_path) {
  return host.plugin.resolve_data &&
         host.plugin.resolve_data(std::string(kind), std::string(leaf),
                                  out_path) &&
         out_path && !out_path->empty();
}

bool capture_path(content::CapabilityHost& host,
                         std::string_view leaf,
                         std::string* out_path) {
  return host.plugin.capture_path &&
         host.plugin.capture_path(std::string(leaf), out_path) && out_path &&
         !out_path->empty();
}

bool sidecar_path(content::CapabilityHost& host,
                         std::string_view rel,
                         std::string* out_path) {
  return host.plugin.sidecar_path &&
         host.plugin.sidecar_path(std::string(rel), out_path) && out_path &&
         !out_path->empty();
}

bool require_plugins(content::CapabilityHost& host) {
  return host.plugin.require_plugins && host.plugin.require_plugins();
}

bool analysis_set_frame(content::CapabilityHost& host, int index) {
  return host.plugin.analysis_set_frame && host.plugin.analysis_set_frame(index);
}

bool analysis_export_frames(content::CapabilityHost& host,
                                   std::string_view dir) {
  return host.plugin.analysis_export_frames &&
         host.plugin.analysis_export_frames(std::string(dir)) > 0;
}

bool open_report(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.plugin.open_report &&
         host.plugin.open_report(std::string(path));
}

bool post_to_report(content::CapabilityHost& host,
                           std::string_view json) {
  return host.plugin.post_to_report &&
         host.plugin.post_to_report(std::string(json));
}


std::string json_escape(const std::string& text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

// args_file bodies are not StrPart-split. $name is resolved against slot_names
// and JSON-escaped the same way the script interpreter expands file text.
std::string expand_file_slots(const std::string& in,
                              const Module& module,
                              const SlotTable& slots) {
  std::string out;
  out.reserve(in.size() + 16);
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] != '$' || i + 1 >= in.size()) {
      out.push_back(in[i]);
      continue;
    }
    const char n0 = in[i + 1];
    if (!((n0 >= 'a' && n0 <= 'z') || (n0 >= 'A' && n0 <= 'Z') || n0 == '_')) {
      out.push_back(in[i]);
      continue;
    }
    std::size_t j = i + 1;
    while (j < in.size()) {
      const char c = in[j];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_') {
        ++j;
        continue;
      }
      break;
    }
    const std::string_view key(in.data() + i + 1, j - (i + 1));
    bool found = false;
    std::string value;
    for (std::size_t s = 0; s < module.slot_names.size(); ++s) {
      if (module.slot_names[s] == key) {
        found = true;
        if (s < slots.values.size()) {
          value = slots.values[s];
        }
        break;
      }
    }
    if (!found) {
      out.push_back(in[i]);
      continue;
    }
    out += json_escape(value);
    i = j - 1;
  }
  return out;
}

bool read_utf8_file(const std::string& path, std::string* out) {
  if (!out || path.empty()) {
    return false;
  }
#if defined(_WIN32)
  const int bytes = static_cast<int>(path.size());
  const int chars =
      MultiByteToWideChar(CP_UTF8, 0, path.data(), bytes, nullptr, 0);
  if (chars <= 0) {
    return false;
  }
  std::wstring wide(static_cast<std::size_t>(chars), L'\0');
  if (MultiByteToWideChar(CP_UTF8, 0, path.data(), bytes, wide.data(), chars) !=
      chars) {
    return false;
  }
  std::ifstream in(wide.c_str(), std::ios::binary);
#else
  std::ifstream in(path, std::ios::binary);
#endif
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  if (out->size() >= 3 && static_cast<unsigned char>((*out)[0]) == 0xEF &&
      static_cast<unsigned char>((*out)[1]) == 0xBB &&
      static_cast<unsigned char>((*out)[2]) == 0xBF) {
    out->erase(0, 3);
  }
  return true;
}

bool eval_run_processing(content::CapabilityHost& host,
                         const Module& module,
                         const Inst& inst,
                         SlotTable& slots,
                         std::string* error) {
  std::string args = arg_str_param(module, inst, slots, 1);
  const std::string args_file = arg_str_param(module, inst, slots, 2);
  if (!args_file.empty()) {
    std::string body;
    if (!read_utf8_file(args_file, &body)) {
      return fail_step(inst, "args_file", error);
    }
    args = expand_file_slots(body, module, slots);
  }
  if (args.empty()) {
    args = "{}";
  }
  return ok_step(inst,
                 run_processing(host, arg_str_param(module, inst, slots, 0), args),
                 error);
}

bool eval_run_plugin_command(content::CapabilityHost& host,
                             const Module& module,
                             const Inst& inst,
                             SlotTable& slots,
                             std::string* error) {
  return ok_step(inst,
                 run_plugin_command(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_resolve_data(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  std::string path;
  if (!resolve_data(host, arg_str_param(module, inst, slots, 0),
                        arg_str_param(module, inst, slots, 1), &path)) {
    return fail_step(inst, "resolve_data", error);
  }
  return store_result(inst, slots, std::move(path), error);
}

bool eval_capture_path(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  std::string path;
  if (!capture_path(host, arg_str_param(module, inst, slots, 0), &path)) {
    return fail_step(inst, "capture_path", error);
  }
  return store_result(inst, slots, std::move(path), error);
}

bool eval_sidecar_path(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  std::string path;
  if (!sidecar_path(host, arg_str_param(module, inst, slots, 0), &path)) {
    return fail_step(inst, "sidecar_path", error);
  }
  return store_result(inst, slots, std::move(path), error);
}

bool eval_require_var(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  std::string value;
  if (const Operand* name = nth_operand(module, inst, 0)) {
    value = slot_from_operand(module, *name, slots);
  }
  if (!value.empty()) {
    return true;
  }
  const std::string token = arg_str_param(module, inst, slots, 1);
  if (!token.empty()) {
    mark(host, token);
  }
  return fail_step(inst, "missing var", error);
}

bool eval_require_plugins(content::CapabilityHost& host,
                          const Module&,
                          const Inst& inst,
                          SlotTable&,
                          std::string* error) {
  return ok_step(inst, require_plugins(host), error);
}

bool eval_analysis_set_frame(content::CapabilityHost& host,
                             const Module& module,
                             const Inst& inst,
                             SlotTable& slots,
                             std::string* error) {
  return ok_step(inst,
                 analysis_set_frame(host, arg_i32_param(module, inst, slots, 0)),
                 error);
}

bool eval_analysis_export_frames(content::CapabilityHost& host,
                                 const Module& module,
                                 const Inst& inst,
                                 SlotTable& slots,
                                 std::string* error) {
  return ok_step(
      inst, analysis_export_frames(host, arg_str_param(module, inst, slots, 0)),
      error);
}

bool eval_open_report(content::CapabilityHost& host,
                      const Module& module,
                      const Inst& inst,
                      SlotTable& slots,
                      std::string* error) {
  return ok_step(inst, open_report(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_post_to_report(content::CapabilityHost& host,
                         const Module& module,
                         const Inst& inst,
                         SlotTable& slots,
                         std::string* error) {
  std::string json;
  if (nth_operand(module, inst, 0)) {
    json = expand_operand(module, *nth_operand(module, inst, 0), slots);
  }
  if (json.empty()) {
    json = arg_str_param(module, inst, slots, 1);
  }
  if (json.empty()) {
    json = "{}";
  }
  return ok_step(inst, post_to_report(host, json), error);
}

constexpr Param kProcessingParams[] = {
    {"id", 0, ArgKind::kStr, false, 0, ""},
    {"args", 1, ArgKind::kStr, false, 0, ""},
    {"args_file", -1, ArgKind::kPath, false, 0, ""},
};
constexpr Param kCommandParams[] = {
    {"id", 0, ArgKind::kStr, false, 0, ""},
};
constexpr Param kResolveParams[] = {
    {"kind", 0, ArgKind::kStr, false, 0, ""},
    {"leaf", 1, ArgKind::kStr, false, 0, ""},
};
constexpr Param kCaptureParams[] = {
    {"leaf", 0, ArgKind::kStr, false, 0, ""},
};
constexpr Param kSidecarParams[] = {
    {"rel", 0, ArgKind::kStr, false, 0, ""},
};
constexpr Param kRequireVarParams[] = {
    {"name", 0, ArgKind::kStr, false, 0, ""},
    {"fail_mark", 1, ArgKind::kStr, false, 0, ""},
};
constexpr Param kFrameParams[] = {
    {"index", 0, ArgKind::kI32, false, 0, nullptr},
};
constexpr Param kExportFramesParams[] = {
    {"dir", 0, ArgKind::kStr, false, 0, "analysis/playback"},
};
constexpr Param kReportParams[] = {
    {"path", 0, ArgKind::kStr, false, 0, ""},
};
constexpr Param kPostParams[] = {
    {"json", 0, ArgKind::kStr, false, 0, ""},
    {"data", -1, ArgKind::kStr, false, 0, "{}"},
};

CONTENT_LANE_OP(RunProcessingOp, Op::kRunProcessing, "run_processing", nullptr,
                kProcessingParams, 3, false, eval_run_processing);
CONTENT_LANE_OP(RunPluginCommandOp, Op::kRunPluginCommand, "run_plugin_command", nullptr,
                kCommandParams, 1, false, eval_run_plugin_command);
CONTENT_LANE_OP(ResolveDataOp, Op::kResolveData, "resolve_data", nullptr, kResolveParams,
                2, true, eval_resolve_data);
CONTENT_LANE_OP(CapturePathOp, Op::kCapturePath, "capture_path", nullptr, kCaptureParams,
                1, true, eval_capture_path);
CONTENT_LANE_OP(SidecarPathOp, Op::kSidecarPath, "sidecar_path", nullptr, kSidecarParams,
                1, true, eval_sidecar_path);
CONTENT_LANE_OP(RequireVarOp, Op::kRequireVar, "require_var", nullptr, kRequireVarParams,
                2, false, eval_require_var);
CONTENT_LANE_OP(RequirePluginsOp, Op::kRequirePlugins, "require_plugins", nullptr,
                nullptr, 0, false, eval_require_plugins);
CONTENT_LANE_OP(AnalysisSetFrameOp, Op::kAnalysisSetFrame, "analysis_set_frame", nullptr,
                kFrameParams, 1, false, eval_analysis_set_frame);
CONTENT_LANE_OP(AnalysisExportFramesOp, Op::kAnalysisExportFrames,
                "analysis_export_frames", nullptr, kExportFramesParams, 1, false,
                eval_analysis_export_frames);
CONTENT_LANE_OP(OpenReportOp, Op::kOpenReport, "open_report", nullptr, kReportParams, 1,
                false, eval_open_report);
CONTENT_LANE_OP(PostToReportOp, Op::kPostToReport, "post_to_report", nullptr, kPostParams,
                2, false, eval_post_to_report);

using PluginOps = std::tuple<RunProcessingOp,
                             RunPluginCommandOp,
                             ResolveDataOp,
                             CapturePathOp,
                             SidecarPathOp,
                             RequireVarOp,
                             RequirePluginsOp,
                             AnalysisSetFrameOp,
                             AnalysisExportFramesOp,
                             OpenReportOp,
                             PostToReportOp>;

}  // namespace

void fill_plugin_ops(const OpDesc** by_op) {
  detail::install_ops<PluginOps>(by_op);
}

}  // namespace code
}  // namespace content

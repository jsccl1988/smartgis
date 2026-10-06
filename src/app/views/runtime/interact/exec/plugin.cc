// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/exec/plugin.h"

#include <string>

#include "app/views/runtime/interact/host/host.h"
#include "app/views/runtime/interact/io/io.h"
#include "app/views/runtime/interact/policy/bind.h"
#include "app/views/runtime/interact/policy/verbs.h"

namespace app {
namespace detail {
namespace {

bool lookup_var(const VarMap* vars, const std::string& key, std::string* out) {
  if (!vars || key.empty() || !out) {
    return false;
  }
  const auto it = vars->find(key);
  if (it == vars->end() || it->second.empty()) {
    return false;
  }
  *out = it->second;
  return true;
}

bool resolve_args_file(content::CapabilityHost& host,
                       VarMap* vars,
                       const std::string& args_file,
                       std::string* path) {
  if (!path) {
    return false;
  }
  if (vars && args_file.size() > 1 && args_file[0] == '$') {
    (void)lookup_var(vars, args_file.substr(1), path);
  } else if (vars) {
    (void)lookup_var(vars, args_file, path);
  }
  if (path->empty() && vars) {
    std::string script_dir;
    if (lookup_var(vars, "script_dir", &script_dir)) {
      const std::string cand = script_dir + "\\" + args_file;
      std::wstring cand_w;
      if (utf8_to_wide(cand, &cand_w) && file_exists_wide(cand_w)) {
        *path = cand;
      }
    }
  }
  if (path->empty() && host.resolve_data) {
    (void)host.resolve_data("harness", args_file, path);
  }
  return !path->empty();
}

std::optional<bool> exec_run_processing(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap* vars) {
  auto p = make_named_tuple("id"_t = std::string(), "args"_t = std::string(),
                            named_only<"args_file"> = std::string());
  bind_into(c, p);
  if (p["id"_t].empty() || !host.run_processing) {
    return false;
  }
  std::string args = p["args"_t];
  if (!p["args_file"_t].empty()) {
    std::string path;
    if (!resolve_args_file(host, vars, p["args_file"_t], &path)) {
      return false;
    }
    std::wstring path_w;
    if (!utf8_to_wide(path, &path_w)) {
      return false;
    }
    std::string file_src;
    if (!read_utf8_file(path_w, &file_src, 1024 * 1024)) {
      return false;
    }
    args = file_src;
  }
  if (args.empty()) {
    args = "{}";
  }
  if (vars) {
    args = expand_vars(args, *vars);
  }
  return host.run_processing(p["id"_t], args);
}

std::optional<bool> exec_resolve_data(content::CapabilityHost& host,
                                      const CallStmt& c,
                                      VarMap* vars) {
  auto p = make_named_tuple("kind"_t = std::string("plugin"),
                            "leaf"_t = std::string(), named_only<"soft"> = 0,
                            named_only<"as"> = std::string());
  bind_into(c, p);
  if (p["as"_t].empty()) {
    return p["soft"_t] != 0;
  }
  if (vars && vars->count(p["as"_t]) && p["soft"_t] != 0) {
    return true;
  }
  if (!host.resolve_data) {
    return p["soft"_t] != 0;
  }
  std::string path;
  const bool ok =
      host.resolve_data(p["kind"_t], p["leaf"_t], &path) && !path.empty();
  if (!ok) {
    return p["soft"_t] != 0;
  }
  return bind_as(vars, c, path);
}

std::optional<bool> exec_capture_path(content::CapabilityHost& host,
                                      const CallStmt& c,
                                      VarMap* vars) {
  auto p = make_named_tuple("leaf"_t = std::string());
  bind_into(c, p);
  std::string path;
  if (!host.capture_path || !host.capture_path(p["leaf"_t], &path) ||
      path.empty()) {
    return false;
  }
  return bind_as(vars, c, path);
}

std::optional<bool> exec_sidecar_path(content::CapabilityHost& host,
                                      const CallStmt& c,
                                      VarMap* vars) {
  auto p = make_named_tuple("rel"_t = std::string());
  bind_into(c, p);
  std::string path;
  if (!host.sidecar_path || !host.sidecar_path(p["rel"_t], &path) ||
      path.empty()) {
    return false;
  }
  return bind_as(vars, c, path);
}

std::optional<bool> exec_require_var(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap* vars) {
  auto p = make_named_tuple("name"_t = std::string(),
                            "fail_mark"_t = std::string());
  bind_into(c, p);
  std::string unused;
  if (!lookup_var(vars, p["name"_t], &unused)) {
    if (!p["fail_mark"_t].empty()) {
      host_mark(host, p["fail_mark"_t].c_str());
    }
    return false;
  }
  return true;
}

std::optional<bool> exec_require_plugins(content::CapabilityHost& host,
                                         const CallStmt&,
                                         VarMap*) {
  return host.require_plugins && host.require_plugins();
}

std::optional<bool> exec_analysis_set_frame(content::CapabilityHost& host,
                                            const CallStmt& c,
                                            VarMap*) {
  if (!host.analysis_set_frame) {
    return false;
  }
  auto p = make_named_tuple("index"_t = 0);
  bind_into(c, p);
  return host.analysis_set_frame(p["index"_t]);
}

std::optional<bool> exec_analysis_export_frames(content::CapabilityHost& host,
                                                const CallStmt& c,
                                                VarMap*) {
  if (!host.analysis_export_frames) {
    return false;
  }
  auto p = make_named_tuple("dir"_t = std::string("analysis/playback"));
  bind_into(c, p);
  return host.analysis_export_frames(p["dir"_t]) > 0;
}

std::optional<bool> exec_open_report(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap* vars) {
  auto p = make_named_tuple("path"_t = std::string());
  bind_into(c, p);
  std::string path = p["path"_t];
  if (vars) {
    path = expand_vars(path, *vars);
  }
  if (path.empty() || !host.open_report) {
    return false;
  }
  return host.open_report(path);
}

std::optional<bool> exec_post_to_report(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap* vars) {
  auto p = make_named_tuple("json"_t = std::string());
  bind_into(c, p);
  std::string json = p["json"_t];
  if (json.empty()) {
    json = arg_ident(c, 0, "data", "{}");
  }
  if (vars) {
    json = expand_vars(json, *vars);
  }
  if (!host.post_to_report) {
    return false;
  }
  return host.post_to_report(json);
}

}  // namespace

std::optional<bool> try_exec_plugin_call(content::CapabilityHost& host,
                                         const CallStmt& c,
                                         VarMap* vars) {
  return dispatch_verbs(
      host, c, vars, verb(exec_run_processing, "run_processing"),
      verb(exec_resolve_data, "resolve_data"),
      verb(exec_capture_path, "capture_path"),
      verb(exec_sidecar_path, "sidecar_path"),
      verb(exec_require_var, "require_var"),
      verb(exec_require_plugins, "require_plugins"),
      verb(exec_analysis_set_frame, "analysis_set_frame"),
      verb(exec_analysis_export_frames, "analysis_export_frames"),
      verb(exec_open_report, "open_report"),
      verb(exec_post_to_report, "post_to_report"));
}

}  // namespace detail
}  // namespace app

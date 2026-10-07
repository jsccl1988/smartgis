// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/lower/plugin.h"

#include <string>

#include "app/views/il.runtime/ir/api.h"
#include "app/views/il.runtime/codegen/io.h"
#include "app/views/il.runtime/codegen/lower/lower_bind.h"
#include "app/views/il.runtime/codegen/lower/ops.h"
#include "app/views/util/charset.h"
#include "plugin/runtime/host/capability/scenario.h"

namespace app {
namespace detail {
namespace {

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
  if (path->empty() && host.plugin.resolve_data) {
    (void)host.plugin.resolve_data("harness", args_file, path);
  }
  return !path->empty();
}

std::optional<Action> lower_run_processing(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("id"_t = std::string(), "args"_t = std::string(),
                            named_only<"args_file"> = std::string());
  bind_into(c, p);
  return Action([p, vars](content::CapabilityHost& host) {
    if (p["id"_t].empty()) {
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
    return ir::run_processing(host, p["id"_t], args);
  });
}

std::optional<Action> lower_resolve_data(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("kind"_t = std::string("plugin"),
                            "leaf"_t = std::string(), named_only<"soft"> = 0,
                            named_only<"as"> = std::string());
  bind_into(c, p);
  return Action([p, vars](content::CapabilityHost& host) {
    if (p["as"_t].empty()) {
      return p["soft"_t] != 0;
    }
    if (vars && vars->count(p["as"_t]) && p["soft"_t] != 0) {
      return true;
    }
    std::string path;
    const bool ok = ir::resolve_data(host, p["kind"_t], p["leaf"_t], &path);
    if (!ok) {
      return p["soft"_t] != 0;
    }
    return bind_as(vars, p["as"_t], path);
  });
}

std::optional<Action> lower_capture_path(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("leaf"_t = std::string());
  bind_into(c, p);
  const std::string as = arg_ident(c, -1, "as", "");
  return Action([p, as, vars](content::CapabilityHost& host) {
    std::string path;
    if (!ir::capture_path(host, p["leaf"_t], &path)) {
      return false;
    }
    return bind_as(vars, as, path);
  });
}

std::optional<Action> lower_sidecar_path(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("rel"_t = std::string());
  bind_into(c, p);
  const std::string as = arg_ident(c, -1, "as", "");
  return Action([p, as, vars](content::CapabilityHost& host) {
    std::string path;
    if (!ir::sidecar_path(host, p["rel"_t], &path)) {
      return false;
    }
    return bind_as(vars, as, path);
  });
}

std::optional<Action> lower_require_var(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("name"_t = std::string(),
                            "fail_mark"_t = std::string());
  bind_into(c, p);
  return Action([p, vars](content::CapabilityHost& host) {
    std::string unused;
    if (!lookup_var(vars, p["name"_t], &unused)) {
      if (!p["fail_mark"_t].empty()) {
        ir::mark(host, p["fail_mark"_t]);
      }
      return false;
    }
    return true;
  });
}

std::optional<Action> lower_require_plugins(const CallStmt&, VarMap*) {
  return Action([](content::CapabilityHost& host) {
    return ir::require_plugins(host);
  });
}

std::optional<Action> lower_analysis_set_frame(const CallStmt& c, VarMap*) {
  auto p = make_named_tuple("index"_t = 0);
  bind_into(c, p);
  return Action([p](content::CapabilityHost& host) {
    return ir::analysis_set_frame(host, p["index"_t]);
  });
}

std::optional<Action> lower_analysis_export_frames(const CallStmt& c, VarMap*) {
  auto p = make_named_tuple("dir"_t = std::string("analysis/playback"));
  bind_into(c, p);
  return Action([p](content::CapabilityHost& host) {
    return ir::analysis_export_frames(host, p["dir"_t]);
  });
}

std::optional<Action> lower_open_report(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("path"_t = std::string());
  bind_into(c, p);
  return Action([p, vars](content::CapabilityHost& host) {
    std::string path = p["path"_t];
    if (vars) {
      path = expand_vars(path, *vars);
    }
    return ir::open_report(host, path);
  });
}

std::optional<Action> lower_post_to_report(const CallStmt& c, VarMap* vars) {
  auto p = make_named_tuple("json"_t = std::string());
  bind_into(c, p);
  std::string json = p["json"_t];
  if (json.empty()) {
    json = arg_ident(c, 0, "data", "{}");
  }
  return Action([json, vars](content::CapabilityHost& host) {
    std::string body = json;
    if (vars) {
      body = expand_vars(body, *vars);
    }
    return ir::post_to_report(host, body);
  });
}

std::optional<Action> lower_run_plugin_command(const CallStmt& c, VarMap*) {
  auto p = make_named_tuple("id"_t = std::string());
  bind_into(c, p);
  return Action([p](content::CapabilityHost& host) {
    return ir::run_plugin_command(host, p["id"_t]);
  });
}

}  // namespace

std::optional<Action> try_lower_plugin_call(const CallStmt& c, VarMap* vars) {
  if (plugin::has_scenario_op(c.name)) {
    auto p = make_named_tuple("mode"_t = std::string());
    bind_into(c, p);
    const std::string name = c.name;
    const std::string mode = p["mode"_t];
    return Action([name, mode](content::CapabilityHost& host) {
      if (std::optional<bool> step =
              plugin::try_exec_scenario_op(host, name, mode)) {
        return *step;
      }
      return true;
    });
  }
  return lower_ops(
      c, vars, op(lower_run_processing, "run_processing"),
      op(lower_run_plugin_command, "run_plugin_command"),
      op(lower_resolve_data, "resolve_data"),
      op(lower_capture_path, "capture_path"),
      op(lower_sidecar_path, "sidecar_path"),
      op(lower_require_var, "require_var"),
      op(lower_require_plugins, "require_plugins"),
      op(lower_analysis_set_frame, "analysis_set_frame"),
      op(lower_analysis_export_frames, "analysis_export_frames"),
      op(lower_open_report, "open_report"),
      op(lower_post_to_report, "post_to_report"));
}

}  // namespace detail
}  // namespace app

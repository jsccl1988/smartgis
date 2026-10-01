// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/interact/exec_shell.h"

#include <cstdio>
#include <string>
#include <windows.h>

#include "app/views/shell/runtime/interact/args.h"
#include "app/views/shell/runtime/interact/host_util.h"
#include "app/views/shell/runtime/interact/os_inject.h"

namespace app {
namespace detail {

std::optional<bool> try_exec_shell_call(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap* vars) {
  const std::string& op = c.name;

  if (op == "pump") {
    host_pump(host, arg_int(c, 0, "ms", 100));
    return true;
  }
  if (op == "select_map_tab") {
    if (host.select_map_tab) {
      host.select_map_tab(arg_int(c, 0, "index", 0));
    }
    return true;
  }
  if (op == "catalog_tab") {
    if (host.catalog_tab) {
      host.catalog_tab(arg_int(c, 0, "index", 0));
    }
    return true;
  }
  if (op == "inspector_tab") {
    if (host.inspector_tab) {
      host.inspector_tab(arg_int(c, 0, "index", 0));
    }
    return true;
  }
  if (op == "mark") {
    host_mark(host, arg_ident(c, 0, "name", "mark").c_str());
    return true;
  }
  if (op == "wait_ready" || op == "wait_map_ready") {
    const int ms = arg_int(c, 0, "ms", 5000);
    const int soft = arg_int(c, -1, "soft", 0);
    const bool ok = host.wait_map_ready && host.wait_map_ready(ms);
    return soft ? true : ok;
  }
  if (op == "load_sample" || op == "load_china_sample") {
    const bool stub = arg_int(c, 0, "stub", 0) != 0;
    return host.load_china_sample && host.load_china_sample(stub);
  }
  if (op == "detach_maps") {
    if (host.detach_maps) {
      host.detach_maps();
    }
    return true;
  }
  if (op == "stop_map_timers") {
    if (host.stop_map_present_timers) {
      host.stop_map_present_timers();
    }
    return true;
  }
  if (op == "clear_marks") {
    if (host.clear_marks) {
      host.clear_marks();
    }
    return true;
  }
  if (op == "require_hwnd") {
    void* hwnd = host.shell_hwnd ? host.shell_hwnd() : nullptr;
    return hwnd != nullptr;
  }
  if (op == "require_edit_host" || op == "expect_host") {
    return host.edit_host_ready && host.edit_host_ready();
  }
  if (op == "tool" || op == "run_tool") {
    const std::string cmd = arg_ident(c, 0, "id", "");
    if (cmd.empty() || !host.run_tool) {
      return false;
    }
    return host.run_tool(cmd);
  }
  if (op == "expect_tool") {
    const std::string want = arg_ident(c, 0, "id", "");
    if (want.empty() || !host.current_tool_id) {
      return false;
    }
    return host.current_tool_id() == want;
  }
  if (op == "expect_geom") {
    const std::string kind = arg_ident(c, 0, "kind", "point");
    const int min_pts = arg_int(c, 1, "min_points", 1);
    return host.expect_last_geom && host.expect_last_geom(kind, min_pts);
  }
  if (op == "browse_stress") {
    const int count = arg_int(c, 0, "count", 24);
    return host.browse_stress && host.browse_stress(count);
  }
  if (op == "expect_wheel_cursor") {
    const int x = arg_int(c, 0, "x", 40);
    const int y = arg_int(c, 1, "y", 40);
    return host.expect_wheel_cursor && host.expect_wheel_cursor(x, y);
  }
  if (op == "map2d_run") {
    const std::string mode = arg_ident(c, 0, "mode", "china");
    return host.map2d_run && host.map2d_run(mode);
  }
  if (op == "atmosphere_run") {
    const std::string mode = arg_ident(c, 0, "mode", "full");
    return host.atmosphere_run && host.atmosphere_run(mode);
  }
  if (op == "plugin_run") {
    const std::string mode = arg_ident(c, 0, "mode", "world3d");
    return host.plugin_run && host.plugin_run(mode);
  }
  if (op == "run_processing") {
    const std::string id = arg_ident(c, 0, "id", "");
    if (id.empty() || !host.run_processing) {
      return false;
    }
    std::string args = arg_ident(c, 1, "args", "");
    const std::string args_file = arg_ident(c, -1, "args_file", "");
    if (!args_file.empty()) {
      std::string path;
      if (vars && args_file.size() > 1 && args_file[0] == '$') {
        const auto it = vars->find(args_file.substr(1));
        if (it != vars->end()) {
          path = it->second;
        }
      } else if (vars && vars->count(args_file)) {
        path = (*vars)[args_file];
      }
      // Prefer file colocated with the running .il (script_dir).
      if (path.empty() && vars && vars->count("script_dir")) {
        const std::string cand = (*vars)["script_dir"] + "\\" + args_file;
        const int wn =
            MultiByteToWideChar(CP_UTF8, 0, cand.c_str(), -1, nullptr, 0);
        if (wn > 1) {
          std::wstring cand_w(static_cast<size_t>(wn - 1), L'\0');
          MultiByteToWideChar(CP_UTF8, 0, cand.c_str(), -1, cand_w.data(), wn);
          if (GetFileAttributesW(cand_w.c_str()) != INVALID_FILE_ATTRIBUTES) {
            path = cand;
          }
        }
      }
      if (path.empty() && host.resolve_data) {
        (void)host.resolve_data("harness", args_file, &path);
      }
      if (path.empty()) {
        return false;
      }
      std::string file_src;
      FILE* f = nullptr;
      const int wn =
          MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
      if (wn <= 1) {
        return false;
      }
      std::wstring path_w(static_cast<size_t>(wn - 1), L'\0');
      MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, path_w.data(), wn);
      if (_wfopen_s(&f, path_w.c_str(), L"rb") != 0 || !f) {
        return false;
      }
      std::fseek(f, 0, SEEK_END);
      const long sz = std::ftell(f);
      std::fseek(f, 0, SEEK_SET);
      if (sz < 0 || sz > 1024 * 1024) {
        std::fclose(f);
        return false;
      }
      file_src.resize(static_cast<size_t>(sz));
      const size_t n = std::fread(file_src.data(), 1, file_src.size(), f);
      std::fclose(f);
      file_src.resize(n);
      args = file_src;
    }
    if (args.empty()) {
      args = "{}";
    }
    if (vars) {
      args = expand_vars(args, *vars);
    }
    return host.run_processing(id, args);
  }
  if (op == "console_run") {
    return host.console_run && host.console_run();
  }
  if (op == "resolve_data") {
    const std::string kind = arg_ident(c, 0, "kind", "plugin");
    const std::string leaf = arg_ident(c, 1, "leaf", "");
    const int soft = arg_int(c, -1, "soft", 0);
    const std::string as = arg_ident(c, -1, "as", "");
    if (as.empty()) {
      return soft != 0;
    }
    if (vars && vars->count(as) && soft != 0) {
      return true;  // keep first hit
    }
    if (!host.resolve_data) {
      return soft != 0;
    }
    std::string path;
    const bool ok = host.resolve_data(kind, leaf, &path) && !path.empty();
    if (!ok) {
      return soft != 0;
    }
    return bind_as(vars, c, path);
  }
  if (op == "capture_path") {
    const std::string leaf = arg_ident(c, 0, "leaf", "");
    std::string path;
    if (!host.capture_path || !host.capture_path(leaf, &path) || path.empty()) {
      return false;
    }
    return bind_as(vars, c, path);
  }
  if (op == "sidecar_path") {
    const std::string rel = arg_ident(c, 0, "rel", "");
    std::string path;
    if (!host.sidecar_path || !host.sidecar_path(rel, &path) || path.empty()) {
      return false;
    }
    return bind_as(vars, c, path);
  }
  if (op == "require_var") {
    const std::string name = arg_ident(c, 0, "name", "");
    const std::string fail_mark = arg_ident(c, 1, "fail_mark", "");
    if (!vars || name.empty() || !vars->count(name) || (*vars)[name].empty()) {
      if (!fail_mark.empty()) {
        host_mark(host, fail_mark.c_str());
      }
      return false;
    }
    return true;
  }
  if (op == "doc_clear") {
    return host.doc_clear && host.doc_clear();
  }
  if (op == "fit_extent") {
    return host.fit_extent && host.fit_extent();
  }
  if (op == "export_bmp") {
    const std::string leaf = arg_ident(c, 0, "leaf", "");
    const std::string frame = arg_ident(c, 1, "frame", "document_extent");
    return host.export_bmp && host.export_bmp(leaf, frame);
  }
  if (op == "suppress_dialogs") {
    const int on = arg_int(c, 0, "on", 1);
    return host.suppress_dialogs && host.suppress_dialogs(on != 0);
  }
  if (op == "require_plugins") {
    return host.require_plugins && host.require_plugins();
  }
  if (op == "apply_style_file") {
    // Prefer var= binding (unescaped filesystem path). path="$name" also ok.
    std::string path;
    const std::string var = arg_ident(c, -1, "var", "");
    if (vars && !var.empty()) {
      const auto it = vars->find(var);
      if (it != vars->end()) {
        path = it->second;
      }
    }
    if (path.empty()) {
      const std::string raw = arg_ident(c, 0, "path", "");
      if (vars && raw.size() > 1 && raw[0] == '$') {
        const auto it = vars->find(raw.substr(1));
        if (it != vars->end()) {
          path = it->second;
        }
      } else {
        path = raw;
      }
    }
    return host.apply_style_file && !path.empty() &&
           host.apply_style_file(path);
  }
  if (op == "invalidate_map2d") {
    return host.invalidate_map2d && host.invalidate_map2d();
  }
  if (op == "analysis_set_frame") {
    if (!host.analysis_set_frame) {
      return false;
    }
    return host.analysis_set_frame(arg_int(c, 0, "index", 0));
  }
  if (op == "analysis_export_frames") {
    if (!host.analysis_export_frames) {
      return false;
    }
    // Nested under captures/analysis/<topic>/ (see build-output.mdc).
    const std::string dir = arg_ident(c, 0, "dir", "analysis/playback");
    const int wrote = host.analysis_export_frames(dir);
    return wrote > 0;
  }
  if (op == "open_report") {
    std::string path = arg_ident(c, 0, "path", "");
    if (vars) {
      path = expand_vars(path, *vars);
    }
    if (path.empty() || !host.open_report) {
      return false;
    }
    return host.open_report(path);
  }
  if (op == "post_to_report") {
    std::string json = arg_ident(c, 0, "json", "");
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
  if (op == "window") {
    const std::string action = arg_ident(c, 0, "action", "activate");
    if (!host.window) {
      return false;
    }
    return host.window(action, arg_int(c, 1, "w", 1280),
                       arg_int(c, 2, "h", 800));
  }
  if (op == "key") {
    const std::string vkname = arg_ident(c, 0, "vk", "");
    WORD vk = vk_from_name(vkname);
    if (!vk) {
      vk = static_cast<WORD>(arg_int(c, 0, "vk", 0));
    }
    return host.key && host.key(vk);
  }

  return std::nullopt;
}

}  // namespace detail
}  // namespace app

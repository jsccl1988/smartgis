// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/exec/document.h"

#include "app/views/runtime/interact/policy/bind.h"
#include "app/views/runtime/interact/policy/verbs.h"

namespace app {
namespace detail {
namespace {

std::optional<bool> exec_load_sample(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap*) {
  auto p = make_named_tuple("stub"_t = 0);
  bind_into(c, p);
  return host.load_china_sample && host.load_china_sample(p["stub"_t] != 0);
}

std::optional<bool> exec_detach_maps(content::CapabilityHost& host,
                                     const CallStmt&,
                                     VarMap*) {
  if (host.detach_maps) {
    host.detach_maps();
  }
  return true;
}

std::optional<bool> exec_stop_map_timers(content::CapabilityHost& host,
                                         const CallStmt&,
                                         VarMap*) {
  if (host.stop_map_present_timers) {
    host.stop_map_present_timers();
  }
  return true;
}

std::optional<bool> exec_clear_marks(content::CapabilityHost& host,
                                     const CallStmt&,
                                     VarMap*) {
  if (host.clear_marks) {
    host.clear_marks();
  }
  return true;
}

std::optional<bool> exec_doc_clear(content::CapabilityHost& host,
                                   const CallStmt&,
                                   VarMap*) {
  return host.doc_clear && host.doc_clear();
}

std::optional<bool> exec_fit_extent(content::CapabilityHost& host,
                                    const CallStmt&,
                                    VarMap*) {
  return host.fit_extent && host.fit_extent();
}

std::optional<bool> exec_export_bmp(content::CapabilityHost& host,
                                    const CallStmt& c,
                                    VarMap*) {
  auto p = make_named_tuple("leaf"_t = std::string(),
                            "frame"_t = std::string("document_extent"));
  bind_into(c, p);
  return host.export_bmp && host.export_bmp(p["leaf"_t], p["frame"_t]);
}

std::optional<bool> exec_suppress_dialogs(content::CapabilityHost& host,
                                          const CallStmt& c,
                                          VarMap*) {
  auto p = make_named_tuple("on"_t = 1);
  bind_into(c, p);
  return host.suppress_dialogs && host.suppress_dialogs(p["on"_t] != 0);
}

std::optional<bool> exec_apply_style_file(content::CapabilityHost& host,
                                          const CallStmt& c,
                                          VarMap* vars) {
  auto p = make_named_tuple("path"_t = std::string(),
                            named_only<"var"> = std::string());
  bind_into(c, p);
  std::string path;
  if (vars && !p["var"_t].empty()) {
    const auto it = vars->find(p["var"_t]);
    if (it != vars->end()) {
      path = it->second;
    }
  }
  if (path.empty()) {
    const std::string& raw = p["path"_t];
    if (vars && raw.size() > 1 && raw[0] == '$') {
      const auto it = vars->find(raw.substr(1));
      if (it != vars->end()) {
        path = it->second;
      }
    } else {
      path = raw;
    }
  }
  return host.apply_style_file && !path.empty() && host.apply_style_file(path);
}

std::optional<bool> exec_invalidate_map2d(content::CapabilityHost& host,
                                          const CallStmt&,
                                          VarMap*) {
  return host.invalidate_map2d && host.invalidate_map2d();
}

}  // namespace

std::optional<bool> try_exec_document_call(content::CapabilityHost& host,
                                           const CallStmt& c,
                                           VarMap* vars) {
  return dispatch_verbs(
      host, c, vars,
      verb(exec_load_sample, "load_sample", "load_china_sample"),
      verb(exec_detach_maps, "detach_maps"),
      verb(exec_stop_map_timers, "stop_map_timers"),
      verb(exec_clear_marks, "clear_marks"), verb(exec_doc_clear, "doc_clear"),
      verb(exec_fit_extent, "fit_extent"), verb(exec_export_bmp, "export_bmp"),
      verb(exec_suppress_dialogs, "suppress_dialogs"),
      verb(exec_apply_style_file, "apply_style_file"),
      verb(exec_invalidate_map2d, "invalidate_map2d"));
}

}  // namespace detail
}  // namespace app

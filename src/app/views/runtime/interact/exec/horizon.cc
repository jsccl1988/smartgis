// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/exec/horizon.h"

#include "app/views/runtime/interact/host/host.h"
#include "app/views/runtime/interact/io/os_inject.h"
#include "app/views/runtime/interact/policy/bind.h"
#include "app/views/runtime/interact/policy/verbs.h"

namespace app {
namespace detail {
namespace {

std::optional<bool> exec_pump(content::CapabilityHost& host,
                              const CallStmt& c,
                              VarMap*) {
  auto p = make_named_tuple("ms"_t = 100);
  bind_into(c, p);
  host_pump(host, p["ms"_t]);
  return true;
}

std::optional<bool> exec_select_map_tab(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap*) {
  auto p = make_named_tuple("index"_t = 0);
  bind_into(c, p);
  if (host.select_map_tab) {
    host.select_map_tab(p["index"_t]);
  }
  return true;
}

std::optional<bool> exec_catalog_tab(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap*) {
  auto p = make_named_tuple("index"_t = 0);
  bind_into(c, p);
  if (host.catalog_tab) {
    host.catalog_tab(p["index"_t]);
  }
  return true;
}

std::optional<bool> exec_inspector_tab(content::CapabilityHost& host,
                                       const CallStmt& c,
                                       VarMap*) {
  auto p = make_named_tuple("index"_t = 0);
  bind_into(c, p);
  if (host.inspector_tab) {
    host.inspector_tab(p["index"_t]);
  }
  return true;
}

std::optional<bool> exec_mark(content::CapabilityHost& host,
                              const CallStmt& c,
                              VarMap*) {
  auto p = make_named_tuple("name"_t = std::string("mark"));
  bind_into(c, p);
  host_mark(host, p["name"_t].c_str());
  return true;
}

std::optional<bool> exec_wait_ready(content::CapabilityHost& host,
                                    const CallStmt& c,
                                    VarMap*) {
  auto p = make_named_tuple("ms"_t = 5000, named_only<"soft"> = 0);
  bind_into(c, p);
  const bool ok = host.wait_map_ready && host.wait_map_ready(p["ms"_t]);
  return p["soft"_t] ? true : ok;
}

std::optional<bool> exec_require_hwnd(content::CapabilityHost& host,
                                      const CallStmt&,
                                      VarMap*) {
  void* hwnd = host.shell_hwnd ? host.shell_hwnd() : nullptr;
  return hwnd != nullptr;
}

std::optional<bool> exec_require_edit_host(content::CapabilityHost& host,
                                           const CallStmt&,
                                           VarMap*) {
  return host.edit_host_ready && host.edit_host_ready();
}

std::optional<bool> exec_window(content::CapabilityHost& host,
                                const CallStmt& c,
                                VarMap*) {
  auto p = make_named_tuple("action"_t = std::string("activate"), "w"_t = 1280,
                            "h"_t = 800);
  bind_into(c, p);
  if (!host.window) {
    return false;
  }
  return host.window(p["action"_t], p["w"_t], p["h"_t]);
}

std::optional<bool> exec_key(content::CapabilityHost& host,
                             const CallStmt& c,
                             VarMap*) {
  auto p = make_named_tuple("vk"_t = std::string());
  bind_into(c, p);
  WORD vk = vk_from_name(p["vk"_t]);
  if (!vk) {
    vk = static_cast<WORD>(arg_int(c, 0, "vk", 0));
  }
  return host.key && host.key(vk);
}

}  // namespace

std::optional<bool> try_exec_horizon_call(content::CapabilityHost& host,
                                          const CallStmt& c,
                                          VarMap* vars) {
  return dispatch_verbs(
      host, c, vars, verb(exec_pump, "pump"),
      verb(exec_select_map_tab, "select_map_tab"),
      verb(exec_catalog_tab, "catalog_tab"),
      verb(exec_inspector_tab, "inspector_tab"), verb(exec_mark, "mark"),
      verb(exec_wait_ready, "wait_ready", "wait_map_ready"),
      verb(exec_require_hwnd, "require_hwnd"),
      verb(exec_require_edit_host, "require_edit_host", "expect_host"),
      verb(exec_window, "window"), verb(exec_key, "key"));
}

}  // namespace detail
}  // namespace app

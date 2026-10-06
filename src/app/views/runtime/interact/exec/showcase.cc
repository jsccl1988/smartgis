// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/exec/showcase.h"

#include "app/views/runtime/interact/policy/bind.h"
#include "app/views/runtime/interact/policy/verbs.h"

namespace app {
namespace detail {
namespace {

std::optional<bool> exec_tool(content::CapabilityHost& host,
                              const CallStmt& c,
                              VarMap*) {
  auto p = make_named_tuple("id"_t = std::string());
  bind_into(c, p);
  if (p["id"_t].empty() || !host.run_tool) {
    return false;
  }
  return host.run_tool(p["id"_t]);
}

std::optional<bool> exec_expect_tool(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap*) {
  auto p = make_named_tuple("id"_t = std::string());
  bind_into(c, p);
  if (p["id"_t].empty() || !host.current_tool_id) {
    return false;
  }
  return host.current_tool_id() == p["id"_t];
}

std::optional<bool> exec_expect_geom(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap*) {
  auto p = make_named_tuple("kind"_t = std::string("point"), "min_points"_t = 1);
  bind_into(c, p);
  return host.expect_last_geom &&
         host.expect_last_geom(p["kind"_t], p["min_points"_t]);
}

std::optional<bool> exec_browse_stress(content::CapabilityHost& host,
                                       const CallStmt& c,
                                       VarMap*) {
  auto p = make_named_tuple("count"_t = 24);
  bind_into(c, p);
  return host.browse_stress && host.browse_stress(p["count"_t]);
}

std::optional<bool> exec_expect_wheel_cursor(content::CapabilityHost& host,
                                             const CallStmt& c,
                                             VarMap*) {
  auto p = make_named_tuple("x"_t = 40, "y"_t = 40);
  bind_into(c, p);
  return host.expect_wheel_cursor &&
         host.expect_wheel_cursor(p["x"_t], p["y"_t]);
}

std::optional<bool> exec_map2d_run(content::CapabilityHost& host,
                                   const CallStmt& c,
                                   VarMap*) {
  auto p = make_named_tuple("mode"_t = std::string("china"));
  bind_into(c, p);
  return host.map2d_run && host.map2d_run(p["mode"_t]);
}

std::optional<bool> exec_atmosphere_run(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap*) {
  auto p = make_named_tuple("mode"_t = std::string("full"));
  bind_into(c, p);
  return host.atmosphere_run && host.atmosphere_run(p["mode"_t]);
}

std::optional<bool> exec_console_run(content::CapabilityHost& host,
                                     const CallStmt&,
                                     VarMap*) {
  return host.console_run && host.console_run();
}

}  // namespace

std::optional<bool> try_exec_showcase_call(content::CapabilityHost& host,
                                           const CallStmt& c,
                                           VarMap* vars) {
  return dispatch_verbs(
      host, c, vars, verb(exec_tool, "tool", "run_tool"),
      verb(exec_expect_tool, "expect_tool"),
      verb(exec_expect_geom, "expect_geom"),
      verb(exec_browse_stress, "browse_stress"),
      verb(exec_expect_wheel_cursor, "expect_wheel_cursor"),
      verb(exec_map2d_run, "map2d_run"),
      verb(exec_atmosphere_run, "atmosphere_run"),
      verb(exec_console_run, "console_run"));
}

}  // namespace detail
}  // namespace app

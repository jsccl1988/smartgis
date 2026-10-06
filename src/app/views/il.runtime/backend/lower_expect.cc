// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/lower_expect.h"

#include <string>

#include "app/views/il.runtime/backend/act.h"
#include "app/views/il.runtime/backend/ops.h"
#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {
namespace {

std::optional<Action> lower_expect_tool(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("id"_t = std::string(), named_only<"fail_rc"> = 0),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::expect_tool(host, p["id"_t], p["fail_rc"_t]);
      });
}

std::optional<Action> lower_expect_geom(const CallStmt& c, VarMap*) {
  return bind_action(
      c, make_named_tuple("kind"_t = std::string("point"), "min_points"_t = 1),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::expect_geom(host, p["kind"_t], p["min_points"_t]);
      });
}

std::optional<Action> lower_expect_wheel_cursor(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("x"_t = 40, "y"_t = 40),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::expect_wheel_cursor(host, p["x"_t], p["y"_t]);
                     });
}

}  // namespace

std::optional<Action> try_lower_expect_call(const CallStmt& c, VarMap* vars) {
  return lower_ops(
      c, vars, op(lower_expect_tool, "expect_tool"),
      op(lower_expect_geom, "expect_geom"),
      op(lower_expect_wheel_cursor, "expect_wheel_cursor"),
      op(lower_host<ir::expect_shell_tree>, "expect_shell_tree"),
      op(lower_host<ir::expect_scene_visible>, "expect_scene_visible"),
      op(lower_host<ir::expect_orbit_moved>, "expect_orbit_moved"),
      op(lower_host<ir::expect_layout_bounds>, "expect_layout_bounds"),
      op(lower_host<ir::expect_map_hwnd_sync>, "expect_map_hwnd_sync"));
}

}  // namespace detail
}  // namespace app

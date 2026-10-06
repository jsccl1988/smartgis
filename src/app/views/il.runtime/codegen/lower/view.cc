// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/lower/view.h"

#include <string>

#include "app/views/il.runtime/codegen/lower/act.h"
#include "app/views/il.runtime/codegen/lower/ops.h"
#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {
namespace {

std::optional<Action> lower_tool(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("id"_t = std::string(), named_only<"fail_rc"> = 0),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::run_tool(host, p["id"_t], p["fail_rc"_t]);
      });
}

std::optional<Action> lower_activate_tool(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("id"_t = std::string()),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::activate_tool(host, p["id"_t]);
                     });
}

std::optional<Action> lower_wait_ready(const CallStmt& c, VarMap*) {
  return bind_action(
      c, make_named_tuple("ms"_t = 5000, named_only<"soft"> = 0),
      [](content::CapabilityHost& host, const auto& p) {
        const bool ok = ir::wait_map_ready(host, p["ms"_t]);
        return p["soft"_t] ? true : ok;
      });
}

std::optional<Action> lower_wait_viewport(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("face"_t = std::string("map"), "ms"_t = 90000,
                       named_only<"frame"> = 1),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::wait_viewport(host, p["face"_t], p["ms"_t], p["frame"_t]);
      });
}

std::optional<Action> lower_browse_stress(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("count"_t = 24),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::browse_stress(host, p["count"_t]);
                     });
}

std::optional<Action> lower_capture_browse_still(const CallStmt& c, VarMap*) {
  return bind_action(c, make_named_tuple("face"_t = std::string("map2d")),
                     [](content::CapabilityHost& host, const auto& p) {
                       return ir::capture_browse_still(host, p["face"_t]);
                     });
}

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

std::optional<Action> lower_camera_fly(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("mode"_t = std::string("orbit"), "ms"_t = 1600,
                       "steps"_t = 20),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::camera_fly(host, p["mode"_t], p["ms"_t], p["steps"_t]);
      });
}

}  // namespace

std::optional<Action> try_lower_view_call(const CallStmt& c, VarMap* vars) {
  return lower_ops(
      c, vars, op(lower_host<ir::detach_maps>, "detach_maps"),
      op(lower_host<ir::stop_map_timers>, "stop_map_timers"),
      op(lower_host<ir::resume_map_timers>, "resume_map_timers"),
      op(lower_host<ir::invalidate_map2d>, "invalidate_map2d"),
      op(lower_tool, "tool", "run_tool"),
      op(lower_activate_tool, "activate_tool"),
      op(lower_wait_ready, "wait_ready", "wait_map_ready"),
      op(lower_wait_viewport, "wait_viewport"),
      op(lower_host<ir::edit_host_ready>, "require_edit_host", "expect_host"),
      op(lower_browse_stress, "browse_stress"),
      op(lower_capture_browse_still, "capture_browse_still"),
      op(lower_host<ir::fps_bench>, "fps_bench"),
      op(lower_host<ir::fit_scene_box>, "fit_scene_box"),
      op(lower_camera_fly, "camera_fly"),
      op(lower_expect_tool, "expect_tool"),
      op(lower_expect_geom, "expect_geom"),
      op(lower_expect_wheel_cursor, "expect_wheel_cursor"),
      op(lower_host<ir::expect_scene_visible>, "expect_scene_visible"),
      op(lower_host<ir::expect_orbit_moved>, "expect_orbit_moved"),
      op(lower_host<ir::expect_map_hwnd_sync>, "expect_map_hwnd_sync"));
}

}  // namespace detail
}  // namespace app

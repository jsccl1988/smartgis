// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/input.h"

#include <string>
#include <vector>

#include "app/views/il.runtime/backend/act.h"
#include "app/views/il.runtime/backend/ops.h"
#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {
namespace {

std::optional<Action> lower_click(const CallStmt& c, VarMap*) {
  const std::string name = c.name;
  return bind_action(
      c,
      make_named_tuple("target"_t = std::string("shell_client"), "x"_t = 0,
                       "y"_t = 0, "button"_t = std::string("left")),
      [name](content::CapabilityHost& host, const auto& p) {
        // rclick / button=right is button 1; dblclick is two clicks.
        const bool right = name == "rclick" || p["button"_t] == "right";
        const int clicks = (name == "dblclick") ? 2 : 1;
        return ir::click(host, p["target"_t], p["x"_t], p["y"_t],
                         right ? 1 : 0, clicks);
      });
}

std::optional<Action> lower_drag(const CallStmt& c, VarMap*) {
  auto p = bind_pack(c, make_named_tuple("target"_t = std::string("shell_client"),
                                         "x0"_t = 0, "y0"_t = 0, "x1"_t = 0,
                                         "y1"_t = 0));
  // Omitted end point stays on the start point.
  p["x1"_t] = arg_int(c, 3, "x1", p["x0"_t]);
  p["y1"_t] = arg_int(c, 4, "y1", p["y0"_t]);
  return host_action(std::move(p), [](content::CapabilityHost& host,
                                      const auto& pack) {
    return ir::drag(host, pack["target"_t], pack["x0"_t], pack["y0"_t],
                    pack["x1"_t], pack["y1"_t]);
  });
}

std::optional<Action> lower_wheel(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("target"_t = std::string("shell_client"), "x"_t = 0,
                       "y"_t = 0, "delta"_t = -120),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::wheel(host, p["target"_t], p["x"_t], p["y"_t], p["delta"_t]);
      });
}

std::optional<Action> lower_path(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("target"_t = std::string("shell_client"),
                       "pts"_t = std::vector<Point>{}),
      [](content::CapabilityHost& host, const auto& p) {
        const std::vector<Point>& pts = p["pts"_t];
        std::vector<int> xs;
        std::vector<int> ys;
        xs.reserve(pts.size());
        ys.reserve(pts.size());
        for (const Point& pt : pts) {
          xs.push_back(pt["x"_t]);
          ys.push_back(pt["y"_t]);
        }
        return ir::stroke(host, p["target"_t], xs, ys);
      });
}

std::optional<Action> lower_pointer(const CallStmt& c, VarMap* vars) {
  return lower_ops(c, vars, op(lower_click, "click", "rclick", "dblclick"),
                   op(lower_drag, "drag"), op(lower_wheel, "wheel"),
                   op(lower_path, "path"));
}

std::optional<Action> lower_pan_burst(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("target"_t = std::string("map_client"),
                       named_only<"count"> = 4, named_only<"x"> = 200,
                       named_only<"y"> = 300, named_only<"dx"> = 40,
                       named_only<"dy"> = 24, named_only<"pump_ms"> = 40),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::pan_burst(host, p["target"_t], p["count"_t], p["x"_t],
                             p["y"_t], p["dx"_t], p["dy"_t], p["pump_ms"_t]);
      });
}

std::optional<Action> lower_wheel_burst(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("target"_t = std::string("map_client"),
                       named_only<"count"> = 3, named_only<"x"> = 400,
                       named_only<"y"> = 400, named_only<"delta"> = -120,
                       named_only<"pump_ms"> = 50),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::wheel_burst(host, p["target"_t], p["count"_t], p["x"_t],
                               p["y"_t], p["delta"_t], p["pump_ms"_t]);
      });
}

std::optional<Action> lower_burst(const CallStmt& c, VarMap* vars) {
  return lower_ops(c, vars, op(lower_pan_burst, "pan_burst"),
                   op(lower_wheel_burst, "wheel_burst"));
}

}  // namespace

std::optional<Action> try_lower_input_call(const CallStmt& c, VarMap* vars) {
  if (std::optional<Action> hit = lower_pointer(c, vars)) {
    return hit;
  }
  return lower_burst(c, vars);
}

}  // namespace detail
}  // namespace app

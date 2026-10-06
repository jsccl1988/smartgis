// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/exec/input.h"

#include <string>
#include <vector>

#include "app/views/runtime/interact/host/host.h"
#include "app/views/runtime/interact/io/os_inject.h"
#include "app/views/runtime/interact/policy/bind.h"
#include "app/views/runtime/interact/policy/verbs.h"
#include "content/public/map_layer_types.h"

namespace app {
namespace detail {
namespace {

using namespace base::tuple::literals;

content::InputEvent make_event(content::InputEvent::Kind kind,
                               int x,
                               int y,
                               int wheel = 0) {
  content::InputEvent e{};
  e.kind = kind;
  e.x_px = x;
  e.y_px = y;
  e.wheel = wheel;
  return e;
}

bool dispatch_drag(content::CapabilityHost& host,
                   int x0,
                   int y0,
                   int x1,
                   int y1) {
  return dispatch_edit(host, make_event(content::InputEvent::Kind::kLDown, x0,
                                        y0)) &&
         dispatch_edit(host, make_event(content::InputEvent::Kind::kMouseMove,
                                        x1, y1)) &&
         dispatch_edit(host,
                       make_event(content::InputEvent::Kind::kLUp, x1, y1));
}

bool is_map_target(std::string_view target) {
  return target == "map_client";
}

std::optional<bool> exec_click(content::CapabilityHost& host,
                               const CallStmt& c,
                               VarMap*) {
  auto p = make_named_tuple("target"_t = std::string("shell_client"),
                            "x"_t = 0, "y"_t = 0,
                            "button"_t = std::string("left"));
  bind_into(c, p);
  HWND hwnd = host_hwnd(host);
  const bool right =
      c.name == "rclick" || p["button"_t] == "right";
  const int x = p["x"_t];
  const int y = p["y"_t];
  auto dispatch_map_ldown = [&](bool rbtn) {
    return dispatch_edit(host,
                         make_event(rbtn ? content::InputEvent::Kind::kRDown
                                         : content::InputEvent::Kind::kLDown,
                                    x, y));
  };
  if (is_map_target(p["target"_t]) && dispatch_map_ldown(right)) {
    if (c.name == "dblclick") {
      dispatch_map_ldown(right);
    }
    return true;
  }
  if (c.name == "dblclick") {
    post_mouse(hwnd, WM_LBUTTONDOWN, WM_LBUTTONUP, x, y);
    return post_mouse(hwnd, WM_LBUTTONDBLCLK, WM_LBUTTONUP, x, y);
  }
  return post_mouse(hwnd, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
                    right ? WM_RBUTTONUP : WM_LBUTTONUP, x, y);
}

std::optional<bool> exec_drag(content::CapabilityHost& host,
                              const CallStmt& c,
                              VarMap*) {
  auto p = make_named_tuple("target"_t = std::string("shell_client"), "x0"_t = 0,
                            "y0"_t = 0, "x1"_t = 0, "y1"_t = 0);
  bind_into(c, p);
  p["x1"_t] = arg_int(c, 3, "x1", p["x0"_t]);
  p["y1"_t] = arg_int(c, 4, "y1", p["y0"_t]);
  const int x0 = p["x0"_t];
  const int y0 = p["y0"_t];
  const int x1 = p["x1"_t];
  const int y1 = p["y1"_t];
  HWND hwnd = host_hwnd(host);
  if (is_map_target(p["target"_t]) && dispatch_drag(host, x0, y0, x1, y1)) {
    return true;
  }
  return post_drag(hwnd, x0, y0, x1, y1);
}

std::optional<bool> exec_wheel(content::CapabilityHost& host,
                               const CallStmt& c,
                               VarMap*) {
  auto p = make_named_tuple("target"_t = std::string("shell_client"), "x"_t = 0,
                            "y"_t = 0, "delta"_t = -120);
  bind_into(c, p);
  HWND hwnd = host_hwnd(host);
  const int x = p["x"_t];
  const int y = p["y"_t];
  const int delta = p["delta"_t];
  if (is_map_target(p["target"_t]) &&
      dispatch_edit(host, make_event(content::InputEvent::Kind::kWheel, x, y,
                                     delta))) {
    return true;
  }
  return post_wheel(hwnd, x, y, delta);
}

std::optional<bool> exec_path(content::CapabilityHost& host,
                              const CallStmt& c,
                              VarMap*) {
  auto p = make_named_tuple("target"_t = std::string("shell_client"),
                            "pts"_t = std::vector<Point>{});
  bind_into(c, p);
  const std::vector<Point>& pts = p["pts"_t];
  if (pts.size() < 2) {
    return false;
  }
  HWND hwnd = host_hwnd(host);
  if (is_map_target(p["target"_t])) {
    if (!dispatch_edit(host, make_event(content::InputEvent::Kind::kLDown,
                                        pts[0]["x"_t], pts[0]["y"_t]))) {
      return post_path(hwnd, pts);
    }
    for (size_t i = 1; i < pts.size(); ++i) {
      if (!dispatch_edit(host,
                         make_event(content::InputEvent::Kind::kMouseMove,
                                    pts[i]["x"_t], pts[i]["y"_t]))) {
        return false;
      }
    }
    return dispatch_edit(
        host, make_event(content::InputEvent::Kind::kLUp, pts.back()["x"_t],
                         pts.back()["y"_t]));
  }
  return post_path(hwnd, pts);
}

std::optional<bool> exec_pan_burst(content::CapabilityHost& host,
                                   const CallStmt& c,
                                   VarMap*) {
  auto p = make_named_tuple("target"_t = std::string("map_client"),
                            named_only<"count"> = 4, named_only<"x"> = 200,
                            named_only<"y"> = 300, named_only<"dx"> = 40,
                            named_only<"dy"> = 24, named_only<"pump_ms"> = 40);
  bind_into(c, p);
  HWND hwnd = host_hwnd(host);
  const bool use_map = is_map_target(p["target"_t]);
  for (int i = 0; i < p["count"_t]; ++i) {
    const int x0 = p["x"_t] + (i % 5) * 8;
    const int y0 = p["y"_t] + (i % 7) * 6;
    const int x1 = x0 + p["dx"_t];
    const int y1 = y0 + p["dy"_t];
    if (use_map) {
      if (!dispatch_drag(host, x0, y0, x1, y1) &&
          !post_drag(hwnd, x0, y0, x1, y1)) {
        return false;
      }
    } else if (!post_drag(hwnd, x0, y0, x1, y1)) {
      return false;
    }
    if (p["pump_ms"_t] > 0) {
      host_pump(host, p["pump_ms"_t]);
    }
  }
  return true;
}

std::optional<bool> exec_wheel_burst(content::CapabilityHost& host,
                                     const CallStmt& c,
                                     VarMap*) {
  auto p = make_named_tuple("target"_t = std::string("map_client"),
                            named_only<"count"> = 3, named_only<"x"> = 400,
                            named_only<"y"> = 400, named_only<"delta"> = -120,
                            named_only<"pump_ms"> = 50);
  bind_into(c, p);
  HWND hwnd = host_hwnd(host);
  const bool use_map = is_map_target(p["target"_t]);
  for (int i = 0; i < p["count"_t]; ++i) {
    const int d = (i % 2 == 0) ? p["delta"_t] : -p["delta"_t];
    if (use_map) {
      if (!dispatch_edit(host, make_event(content::InputEvent::Kind::kWheel,
                                          p["x"_t], p["y"_t], d)) &&
          !post_wheel(hwnd, p["x"_t], p["y"_t], d)) {
        return false;
      }
    } else if (!post_wheel(hwnd, p["x"_t], p["y"_t], d)) {
      return false;
    }
    if (p["pump_ms"_t] > 0) {
      host_pump(host, p["pump_ms"_t]);
    }
  }
  return true;
}

}  // namespace

std::optional<bool> try_exec_input_call(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap* vars) {
  return dispatch_verbs(
      host, c, vars, verb(exec_click, "click", "rclick", "dblclick"),
      verb(exec_drag, "drag"), verb(exec_wheel, "wheel"),
      verb(exec_path, "path"), verb(exec_pan_burst, "pan_burst"),
      verb(exec_wheel_burst, "wheel_burst"));
}

}  // namespace detail
}  // namespace app

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/interact/exec_input.h"

#include <string>
#include <vector>

#include "app/views/shell/runtime/interact/args.h"
#include "app/views/shell/runtime/interact/host_util.h"
#include "app/views/shell/runtime/interact/os_inject.h"
#include "content/public/map_types.h"

namespace app {
namespace detail {

std::optional<bool> try_exec_input_call(content::CapabilityHost& host,
                                        const CallStmt& c,
                                        VarMap* vars) {
  (void)vars;
  HWND hwnd = host_hwnd(host);
  const std::string& op = c.name;

  const std::string target = arg_ident(c, 0, "target", "shell_client");
  const bool map = (target == "map_client");

  auto dispatch_map_ldown = [&](int x, int y, bool right) {
    content::InputEvent e{};
    e.kind = right ? content::InputEvent::Kind::kRDown
                   : content::InputEvent::Kind::kLDown;
    e.x_px = x;
    e.y_px = y;
    return dispatch_edit(host, e);
  };

  if (op == "click" || op == "rclick" || op == "dblclick") {
    const int x = arg_int(c, 1, "x", 0);
    const int y = arg_int(c, 2, "y", 0);
    const std::string button = arg_ident(c, 3, "button", "left");
    const bool right = op == "rclick" || button == "right";
    if (map && dispatch_map_ldown(x, y, right)) {
      if (op == "dblclick") {
        dispatch_map_ldown(x, y, right);
      }
      return true;
    }
    if (op == "dblclick") {
      post_mouse(hwnd, WM_LBUTTONDOWN, WM_LBUTTONUP, x, y);
      return post_mouse(hwnd, WM_LBUTTONDBLCLK, WM_LBUTTONUP, x, y);
    }
    return post_mouse(hwnd, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
                      right ? WM_RBUTTONUP : WM_LBUTTONUP, x, y);
  }
  if (op == "drag") {
    const int x0 = arg_int(c, 1, "x0", 0);
    const int y0 = arg_int(c, 2, "y0", 0);
    const int x1 = arg_int(c, 3, "x1", x0);
    const int y1 = arg_int(c, 4, "y1", y0);
    if (map) {
      content::InputEvent down{};
      down.kind = content::InputEvent::Kind::kLDown;
      down.x_px = x0;
      down.y_px = y0;
      content::InputEvent move{};
      move.kind = content::InputEvent::Kind::kMouseMove;
      move.x_px = x1;
      move.y_px = y1;
      content::InputEvent up{};
      up.kind = content::InputEvent::Kind::kLUp;
      up.x_px = x1;
      up.y_px = y1;
      if (dispatch_edit(host, down) && dispatch_edit(host, move) &&
          dispatch_edit(host, up)) {
        return true;
      }
    }
    return post_drag(hwnd, x0, y0, x1, y1);
  }
  if (op == "wheel") {
    const int x = arg_int(c, 1, "x", 0);
    const int y = arg_int(c, 2, "y", 0);
    const int delta = arg_int(c, 3, "delta", -120);
    if (map) {
      content::InputEvent e{};
      e.kind = content::InputEvent::Kind::kWheel;
      e.x_px = x;
      e.y_px = y;
      e.wheel = delta;
      if (dispatch_edit(host, e)) {
        return true;
      }
    }
    return post_wheel(hwnd, x, y, delta);
  }
  if (op == "path") {
    std::vector<Point> pts = arg_points(c);
    if (pts.size() < 2) {
      return false;
    }
    if (map) {
      content::InputEvent down{};
      down.kind = content::InputEvent::Kind::kLDown;
      down.x_px = pts[0].x;
      down.y_px = pts[0].y;
      if (!dispatch_edit(host, down)) {
        return post_path(hwnd, pts);
      }
      for (size_t i = 1; i < pts.size(); ++i) {
        content::InputEvent move{};
        move.kind = content::InputEvent::Kind::kMouseMove;
        move.x_px = pts[i].x;
        move.y_px = pts[i].y;
        if (!dispatch_edit(host, move)) {
          return false;
        }
      }
      content::InputEvent up{};
      up.kind = content::InputEvent::Kind::kLUp;
      up.x_px = pts.back().x;
      up.y_px = pts.back().y;
      return dispatch_edit(host, up);
    }
    return post_path(hwnd, pts);
  }
  if (op == "pan_burst") {
    const int count = arg_int(c, -1, "count", 4);
    const int x = arg_int(c, -1, "x", 200);
    const int y = arg_int(c, -1, "y", 300);
    const int dx = arg_int(c, -1, "dx", 40);
    const int dy = arg_int(c, -1, "dy", 24);
    const int between = arg_int(c, -1, "pump_ms", 40);
    const std::string tgt = arg_ident(c, 0, "target", "map_client");
    const bool use_map = (tgt == "map_client");
    for (int i = 0; i < count; ++i) {
      const int x0 = x + (i % 5) * 8;
      const int y0 = y + (i % 7) * 6;
      const int x1 = x0 + dx;
      const int y1 = y0 + dy;
      if (use_map) {
        content::InputEvent down{};
        down.kind = content::InputEvent::Kind::kLDown;
        down.x_px = x0;
        down.y_px = y0;
        content::InputEvent move{};
        move.kind = content::InputEvent::Kind::kMouseMove;
        move.x_px = x1;
        move.y_px = y1;
        content::InputEvent up{};
        up.kind = content::InputEvent::Kind::kLUp;
        up.x_px = x1;
        up.y_px = y1;
        if (!(dispatch_edit(host, down) && dispatch_edit(host, move) &&
              dispatch_edit(host, up))) {
          if (!post_drag(hwnd, x0, y0, x1, y1)) {
            return false;
          }
        }
      } else if (!post_drag(hwnd, x0, y0, x1, y1)) {
        return false;
      }
      if (between > 0) {
        host_pump(host, between);
      }
    }
    return true;
  }
  if (op == "wheel_burst") {
    const int count = arg_int(c, -1, "count", 3);
    const int x = arg_int(c, -1, "x", 400);
    const int y = arg_int(c, -1, "y", 400);
    const int delta = arg_int(c, -1, "delta", -120);
    const int between = arg_int(c, -1, "pump_ms", 50);
    const std::string tgt = arg_ident(c, 0, "target", "map_client");
    const bool use_map = (tgt == "map_client");
    for (int i = 0; i < count; ++i) {
      const int d = (i % 2 == 0) ? delta : -delta;
      if (use_map) {
        content::InputEvent e{};
        e.kind = content::InputEvent::Kind::kWheel;
        e.x_px = x;
        e.y_px = y;
        e.wheel = d;
        if (!dispatch_edit(host, e) && !post_wheel(hwnd, x, y, d)) {
          return false;
        }
      } else if (!post_wheel(hwnd, x, y, d)) {
        return false;
      }
      if (between > 0) {
        host_pump(host, between);
      }
    }
    return true;
  }

  return std::nullopt;
}

}  // namespace detail
}  // namespace app

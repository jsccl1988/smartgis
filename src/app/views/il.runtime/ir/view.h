// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_VIEW_H_
#define IL_RUNTIME_IR_VIEW_H_

#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "app/views/il.runtime/ir/fail.h"

namespace app {
namespace ir {

// Any view face: present, edit input, tools, and load facts. Only host.view.

inline void detach_maps(content::CapabilityHost& host) {
  if (host.view.detach_maps) {
    host.view.detach_maps();
  }
}

inline void stop_map_timers(content::CapabilityHost& host) {
  if (host.view.stop_map_present_timers) {
    host.view.stop_map_present_timers();
  }
}

inline void resume_map_timers(content::CapabilityHost& host) {
  if (host.view.resume_map_present_timers) {
    host.view.resume_map_present_timers();
  }
}

inline bool invalidate_map2d(content::CapabilityHost& host) {
  return host.view.invalidate_map2d && host.view.invalidate_map2d();
}

inline bool dispatch_edit(content::CapabilityHost& host,
                          const content::InputEvent& event) {
  return host.view.dispatch_edit_input && host.view.dispatch_edit_input(event);
}

inline bool release_exclusive(content::CapabilityHost& host) {
  return host.view.release_exclusive && host.view.release_exclusive();
}

inline bool run_tool(content::CapabilityHost& host,
                     std::string_view id,
                     int fail_rc) {
  if (id.empty() || !host.view.run_tool) {
    return note_fail(host, false, fail_rc);
  }
  return note_fail(host, host.view.run_tool(std::string(id)), fail_rc);
}

inline bool activate_tool(content::CapabilityHost& host, std::string_view id) {
  if (id.empty() || !host.view.activate_tool) {
    return false;
  }
  return apply_rc(host, host.view.activate_tool(std::string(id)));
}

inline std::string_view canonical_geom_kind(std::string_view kind) {
  if (kind == "line") {
    return "linestring";
  }
  if (kind == "poly") {
    return "polygon";
  }
  return kind;
}

// IL registers |cb|. Later matching reads notify it with the snapshot.
inline void watch_app_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::ViewLoadStatus&)> cb) {
  host.view.on_app_event = std::move(cb);
}

inline void watch_geom_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::GeomStatus&)> cb) {
  host.view.on_geom_event = std::move(cb);
}

inline void watch_scale_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::ViewScaleStatus&)>
        cb) {
  host.view.on_scale_event = std::move(cb);
}

inline void watch_map_ready_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::MapReadyStatus&)>
        cb) {
  host.view.on_map_ready_event = std::move(cb);
}

inline void watch_edit_host_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::EditHostStatus&)>
        cb) {
  host.view.on_edit_host_event = std::move(cb);
}

inline void watch_tool_event(
    content::CapabilityHost& host,
    std::function<void(const std::string&, const content::ToolStatus&)> cb) {
  host.view.on_tool_event = std::move(cb);
}

// Read facts, then notify a registered callback. The caller checks |out|.

inline bool read_last_geom(content::CapabilityHost& host,
                           content::GeomStatus* out) {
  if (!out || !host.view.last_geom || !host.view.last_geom(out)) {
    return false;
  }
  if (host.view.on_geom_event) {
    host.view.on_geom_event("view.geom", *out);
  }
  return true;
}

inline bool read_view_scale(content::CapabilityHost& host,
                            content::ViewScaleStatus* out) {
  if (!out || !host.view.view_scale || !host.view.view_scale(out)) {
    return false;
  }
  if (host.view.on_scale_event) {
    host.view.on_scale_event("view.scale", *out);
  }
  return true;
}

inline bool read_map_ready(content::CapabilityHost& host,
                           int timeout_ms,
                           content::MapReadyStatus* out) {
  if (!out || !host.view.map_ready_status ||
      !host.view.map_ready_status(timeout_ms, out)) {
    return false;
  }
  if (host.view.on_map_ready_event) {
    host.view.on_map_ready_event("map.ready", *out);
  }
  return true;
}

inline bool read_edit_host(content::CapabilityHost& host,
                           content::EditHostStatus* out) {
  if (!out || !host.view.edit_host_status ||
      !host.view.edit_host_status(out)) {
    return false;
  }
  if (host.view.on_edit_host_event) {
    host.view.on_edit_host_event("view.edit_host", *out);
  }
  return true;
}

inline bool read_tool_status(content::CapabilityHost& host,
                             content::ToolStatus* out) {
  if (!out || !host.view.tool_status || !host.view.tool_status(out)) {
    return false;
  }
  if (host.view.on_tool_event) {
    host.view.on_tool_event("view.tool", *out);
  }
  return true;
}

inline bool read_view_load(content::CapabilityHost& host,
                           std::string_view face,
                           int timeout_ms,
                           content::ViewLoadStatus* out) {
  if (!out || !host.view.load_status ||
      !host.view.load_status(std::string(face), timeout_ms, out)) {
    return false;
  }
  if (host.view.on_app_event) {
    host.view.on_app_event("map.load", *out);
  }
  return true;
}

// Layout-build wait. Soft scripts keep going when layout_built is 0.
inline bool wait_map_ready(content::CapabilityHost& host, int timeout_ms) {
  content::MapReadyStatus status;
  if (!read_map_ready(host, timeout_ms, &status)) {
    return false;
  }
  return status.layout_built != 0;
}

// Memory edit session present on the edit host.
inline bool edit_host_ready(content::CapabilityHost& host) {
  content::EditHostStatus status;
  if (!read_edit_host(host, &status)) {
    return false;
  }
  return status.memory_session != 0;
}

inline bool expect_tool(content::CapabilityHost& host,
                        std::string_view id,
                        int fail_rc) {
  if (id.empty()) {
    return note_fail(host, false, fail_rc);
  }
  content::ToolStatus status;
  if (!read_tool_status(host, &status)) {
    return note_fail(host, false, fail_rc);
  }
  return note_fail(host, status.id == id, fail_rc);
}

// Digitize gate. Kind aliases "line" / "poly" match the script verbs.
inline bool expect_geom(content::CapabilityHost& host,
                        std::string_view kind,
                        int min_points) {
  content::GeomStatus got;
  if (!read_last_geom(host, &got)) {
    return false;
  }
  const std::string_view want = canonical_geom_kind(kind);
  const int need = min_points > 0 ? min_points : 1;
  return got.is_append != 0 && !got.kind.empty() && got.kind == want &&
         got.point_count >= need;
}

// Dispatch a cursor wheel, then compare scale before and after. Ortho is a
// fact on the scale snapshot. A missing frame records a mark and fails.
inline bool expect_wheel_cursor(content::CapabilityHost& host, int x, int y) {
  content::ViewScaleStatus before;
  if (!read_view_scale(host, &before)) {
    return false;
  }
  if (!before.has_frame) {
    if (host.horizon.mark) {
      host.horizon.mark("wheel-cursor-no-frame");
    }
    return false;
  }
  const auto send = [&](int delta) {
    content::InputEvent wheel{};
    wheel.kind = content::InputEvent::Kind::kWheel;
    wheel.x_px = x;
    wheel.y_px = y;
    wheel.wheel = delta;
    return dispatch_edit(host, wheel);
  };
  if (!send(-120)) {
    return false;
  }
  content::ViewScaleStatus after;
  if (!read_view_scale(host, &after)) {
    return false;
  }
  if (std::fabs(after.scale - before.scale) < 1e-9) {
    if (!send(120) || !read_view_scale(host, &after) ||
        std::fabs(after.scale - before.scale) < 1e-9) {
      return false;
    }
  }
  return after.ortho != 0;
}

inline bool browse_stress(content::CapabilityHost& host, int count) {
  return host.view.browse_stress && host.view.browse_stress(count);
}

inline bool fps_bench(content::CapabilityHost& host) {
  return host.view.fps_bench && host.view.fps_bench();
}

inline bool capture_browse_still(content::CapabilityHost& host,
                                 std::string_view face) {
  return host.view.capture_browse_still &&
         host.view.capture_browse_still(std::string(face));
}

inline bool fit_scene_box(content::CapabilityHost& host) {
  return host.view.fit_scene_box && host.view.fit_scene_box();
}

inline bool camera_fly(content::CapabilityHost& host, std::string_view mode,
                       int ms, int steps) {
  return host.view.camera_fly &&
         host.view.camera_fly(std::string(mode), ms, steps);
}

// Wait until the face has a real pane, then IL checks the snapshot.
// Map miss → 3. Scene pane miss → 9. Scene frame miss → 10.
// A pane that is not a ContentMapView is not a failure.
inline bool wait_viewport(content::CapabilityHost& host,
                          std::string_view face,
                          int timeout_ms,
                          int want_frame) {
  content::ViewLoadStatus status;
  if (!read_view_load(host, face, timeout_ms, &status)) {
    return false;
  }
  const bool scene = face == "scene" || face == "scene3d";
  if (!status.pane_present) {
    return apply_rc(host, scene ? 9 : 3);
  }
  if (!status.content_view) {
    return true;
  }
  if (!status.hwnd_alive) {
    return apply_rc(host, scene ? 9 : 3);
  }
  if (want_frame && !status.frame_ready) {
    return apply_rc(host, scene ? 10 : 3);
  }
  return true;
}

inline bool expect_scene_visible(content::CapabilityHost& host) {
  content::ViewLoadStatus scene;
  content::ViewLoadStatus map;
  if (!read_view_load(host, "scene", 0, &scene) ||
      !read_view_load(host, "map", 0, &map)) {
    return false;
  }
  if (!scene.hwnd_alive) {
    return apply_rc(host, 9);
  }
  if (map.hwnd_alive && map.visible) {
    return apply_rc(host, 36);
  }
  if (!scene.visible) {
    return apply_rc(host, 37);
  }
  return true;
}

inline bool expect_orbit_moved(content::CapabilityHost& host) {
  content::ViewLoadStatus status;
  if (!read_view_load(host, "scene", 0, &status)) {
    return false;
  }
  if (!status.orbit_moved) {
    return apply_rc(host, 25);
  }
  return true;
}

inline bool expect_map_hwnd_sync(content::CapabilityHost& host) {
  content::ViewLoadStatus status;
  if (!read_view_load(host, "map", 0, &status)) {
    return false;
  }
  const auto fail = [&](int rc) {
    detach_maps(host);
    return apply_rc(host, rc);
  };
  if (status.view_w <= 0 || status.view_h <= 0) {
    return fail(31);
  }
  if (!status.hwnd_alive) {
    return fail(35);
  }
  constexpr int kTol = 2;
  if (std::abs(status.hwnd_x - status.view_x) > kTol ||
      std::abs(status.hwnd_y - status.view_y) > kTol) {
    return fail(33);
  }
  if (std::abs(status.hwnd_w - status.view_w) > kTol ||
      std::abs(status.hwnd_h - status.view_h) > kTol) {
    return fail(34);
  }
  if (status.menu_min_px > 0 && status.menu_h < status.menu_min_px) {
    return fail(32);
  }
  return true;
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_VIEW_H_

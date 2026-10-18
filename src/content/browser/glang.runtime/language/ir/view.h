// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_VIEW_H_
#define IL_RUNTIME_IR_VIEW_H_

#include <cmath>
#include <string>
#include <string_view>
#include <utility>

#include "content/browser/glang.runtime/language/ir/fail.h"

namespace content {
namespace ir {

// Any view face: present, edit input, tools, and load facts. Only host.view.

inline void detach_views(content::CapabilityHost& host) {
  if (host.view.detach_views) {
    host.view.detach_views();
  }
}

inline void stop_map_timers(content::CapabilityHost& host) {
  if (host.view.stop_present_timers) {
    host.view.stop_present_timers();
  }
}

inline void resume_map_timers(content::CapabilityHost& host) {
  if (host.view.resume_present_timers) {
    host.view.resume_present_timers();
  }
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
template <content::detail::op_callable<void, const std::string&, const content::ViewLoadStatus&> F>
inline void watch_app_event(content::CapabilityHost& host, F&& cb) {
  host.view.on_app_event = std::forward<F>(cb);
}

template <content::detail::op_callable<void, const std::string&, const content::GeomStatus&> F>
inline void watch_geom_event(content::CapabilityHost& host, F&& cb) {
  host.view.on_geom_event = std::forward<F>(cb);
}

template <content::detail::op_callable<void, const std::string&, const content::ViewScaleStatus&> F>
inline void watch_scale_event(content::CapabilityHost& host, F&& cb) {
  host.view.on_scale_event = std::forward<F>(cb);
}

template <content::detail::op_callable<void, const std::string&, const content::MapReadyStatus&> F>
inline void watch_map_ready_event(content::CapabilityHost& host, F&& cb) {
  host.view.on_map_ready_event = std::forward<F>(cb);
}

template <content::detail::op_callable<void,
                                       const std::string&,
                                       const content::EditToolSessionStatus&> F>
inline void watch_edit_tool_session_event(content::CapabilityHost& host, F&& cb) {
  host.view.on_edit_tool_session_event = std::forward<F>(cb);
}

template <content::detail::op_callable<void, const std::string&, const content::ToolStatus&> F>
inline void watch_tool_event(content::CapabilityHost& host, F&& cb) {
  host.view.on_tool_event = std::forward<F>(cb);
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

inline bool read_edit_tool_session(content::CapabilityHost& host,
                           content::EditToolSessionStatus* out) {
  if (!out || !host.view.edit_tool_session_status ||
      !host.view.edit_tool_session_status(out)) {
    return false;
  }
  if (host.view.on_edit_tool_session_event) {
    host.view.on_edit_tool_session_event("view.edit_tool_session", *out);
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
inline bool edit_tool_session_ready(content::CapabilityHost& host) {
  content::EditToolSessionStatus status;
  if (!read_edit_tool_session(host, &status)) {
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

}  // namespace ir
}  // namespace content

#endif  // IL_RUNTIME_IR_VIEW_H_

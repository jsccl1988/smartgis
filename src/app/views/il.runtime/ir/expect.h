// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_EXPECT_H_
#define IL_RUNTIME_IR_EXPECT_H_

#include <string>
#include <string_view>

#include "app/views/il.runtime/ir/horizon.h"
#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// Viewport / tool / geom / DebugAgent gates used by testing/*.il.

inline bool run_tool(content::CapabilityHost& host,
                     std::string_view id,
                     int fail_rc) {
  if (id.empty() || !host.run_tool) {
    return note_fail(host, false, fail_rc);
  }
  return note_fail(host, host.run_tool(std::string(id)), fail_rc);
}

inline bool expect_tool(content::CapabilityHost& host,
                        std::string_view id,
                        int fail_rc) {
  if (id.empty() || !host.current_tool_id) {
    return note_fail(host, false, fail_rc);
  }
  return note_fail(host, host.current_tool_id() == id, fail_rc);
}

inline bool expect_geom(content::CapabilityHost& host,
                        std::string_view kind,
                        int min_points) {
  return host.expect_last_geom &&
         host.expect_last_geom(std::string(kind), min_points);
}

inline bool expect_wheel_cursor(content::CapabilityHost& host, int x, int y) {
  return host.expect_wheel_cursor && host.expect_wheel_cursor(x, y);
}

inline bool wait_viewport(content::CapabilityHost& host,
                          std::string_view face,
                          int timeout_ms,
                          int want_frame) {
  if (!host.wait_viewport) {
    return false;
  }
  return apply_rc(host, host.wait_viewport(std::string(face), timeout_ms,
                                           want_frame));
}

inline bool expect_shell_tree(content::CapabilityHost& host) {
  return host.expect_shell_tree && apply_rc(host, host.expect_shell_tree());
}

inline bool expect_scene_visible(content::CapabilityHost& host) {
  return host.expect_scene_visible &&
         apply_rc(host, host.expect_scene_visible());
}

inline bool expect_orbit_moved(content::CapabilityHost& host) {
  return host.expect_orbit_moved && apply_rc(host, host.expect_orbit_moved());
}

inline bool expect_layout_bounds(content::CapabilityHost& host) {
  return host.expect_layout_bounds &&
         apply_rc(host, host.expect_layout_bounds());
}

inline bool expect_map_hwnd_sync(content::CapabilityHost& host) {
  return host.expect_map_hwnd_sync &&
         apply_rc(host, host.expect_map_hwnd_sync());
}

inline bool activate_tool(content::CapabilityHost& host, std::string_view id) {
  if (id.empty() || !host.activate_tool) {
    return false;
  }
  return apply_rc(host, host.activate_tool(std::string(id)));
}

inline bool wire_debug_agent(content::CapabilityHost& host) {
  return host.wire_debug_agent && apply_rc(host, host.wire_debug_agent());
}

inline bool debug_exec(content::CapabilityHost& host,
                       std::string_view line,
                       std::string_view contains,
                       std::string_view equals,
                       std::string_view reject,
                       int fail_rc) {
  if (line.empty() || !host.debug_exec) {
    return false;
  }
  return apply_rc(host, host.debug_exec(std::string(line), std::string(contains),
                                        std::string(equals), std::string(reject),
                                        fail_rc));
}

inline bool console_pan_bench(content::CapabilityHost& host) {
  return host.console_pan_bench && apply_rc(host, host.console_pan_bench());
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_EXPECT_H_

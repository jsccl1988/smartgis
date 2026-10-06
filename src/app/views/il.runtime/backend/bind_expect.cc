// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/bind_horizon.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/bind/slots.h"
#include "app/views/il.runtime/backend/debug_console.h"
#include "app/views/il.runtime/backend/shell_expect.h"
#include "app/views/il.runtime/backend/probe.h"

namespace app {
namespace detail {

void bind_expect(Browser& browser,
                 content::CapabilityHost* out,
                 const wchar_t* mark_leaf) {
  Browser* b = &browser;
  const wchar_t* leaf = mark_leaf;

  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_wait_viewport> =
                   [b](const std::string& face, int timeout_ms, int want_frame) {
                     return wait_content_viewport(*b, face, timeout_ms,
                                                  want_frame != 0);
                   },
               base::tag_resolver<slot_wait_map_ready> =
                   [b](int timeout_ms) { return wait_map_ready(*b, timeout_ms); },
               base::tag_resolver<slot_edit_host_ready> =
                   [b]() { return edit_host_ready(*b); },
               base::tag_resolver<slot_current_tool_id> =
                   [b]() { return current_tool_id(*b); },
               base::tag_resolver<slot_expect_last_geom> =
                   [b](const std::string& kind, int min_points) {
                     return expect_last_geom(*b, kind, min_points);
                   },
               base::tag_resolver<slot_expect_wheel_cursor> =
                   [b, leaf](int x, int y) {
                     return expect_wheel_cursor(*b, leaf, x, y);
                   },
               base::tag_resolver<slot_expect_shell_tree> =
                   [b]() { return expect_shell_tree(*b); },
               base::tag_resolver<slot_expect_scene_visible> =
                   [b]() { return expect_scene_visible(*b); },
               base::tag_resolver<slot_expect_orbit_moved> =
                   [b]() { return expect_orbit_moved(*b); },
               base::tag_resolver<slot_expect_layout_bounds> =
                   [b]() { return expect_layout_bounds(*b); },
               base::tag_resolver<slot_expect_map_hwnd_sync> =
                   [b]() { return expect_map_hwnd_sync(*b); },
               base::tag_resolver<slot_activate_tool> =
                   [b](const std::string& id) {
                     return activate_view_tool(*b, id);
                   },
               base::tag_resolver<slot_wire_debug_agent> =
                   [b]() { return wire_debug_agent(*b); },
               base::tag_resolver<slot_debug_exec> =
                   [b](const std::string& line, const std::string& contains,
                       const std::string& equals, const std::string& reject,
                       int fail_rc) {
                     return debug_exec(*b, line, contains, equals, reject,
                                       fail_rc);
                   },
               base::tag_resolver<slot_console_pan_bench> =
                   [b, leaf]() { return console_pan_bench(*b, leaf); },
           });
}

}  // namespace detail
}  // namespace app

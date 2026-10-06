// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_PROBE_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_PROBE_H_

#include <string>

namespace content {
class ViewHost;
}  // namespace content

namespace ui {
namespace views {
class DrawHost;
}
}  // namespace ui

namespace app {

class Browser;

namespace detail {

// UI-active map host, or the edit host when the shell has no active view.
content::ViewHost* active_map_host(Browser& browser);

int activate_view_tool(Browser& browser, const std::string& id);
bool edit_host_ready(Browser& browser);
bool wait_map_ready(Browser& browser, int timeout_ms);
std::string current_tool_id(Browser& browser);
bool expect_last_geom(Browser& browser, const std::string& kind, int min_points);
bool expect_wheel_cursor(Browser& browser, const wchar_t* leaf, int x, int y);

// True when the pane's latest SharedSurface is a real presented frame.
bool viewport_has_presented_frame(ui::views::DrawHost* pane);

// ContentMapView wait_ready (+ optional SharedSurface). |face| is "map" or
// "scene". Map miss/timeout → 3; scene HWND miss → 9; scene timeout → 10.
// A pane that is not a ContentMapView returns 0.
int wait_content_viewport(Browser& browser,
                          const std::string& face,
                          int timeout_ms,
                          bool want_frame);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_PROBE_H_

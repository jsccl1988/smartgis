// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_PROBE_H_
#define IL_RUNTIME_BACKEND_VIEW_PROBE_H_

#include <string>

#include "content/browser/capability/view.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

namespace content {
class ToolSession;
}  // namespace content

namespace content {


namespace detail {

// UI-active map host, or the edit host when the shell has no active view.
content::ToolSession* active_map_host(content::Shell& browser);

int activate_view_tool(content::Shell& browser, const std::string& id);

// Edit-host presence flags. No pass/fail.
bool fill_edit_tool_session_status(content::Shell& browser, content::EditToolSessionStatus* out);
// Map2d layout readiness. |timeout_ms| > 0 waits for a layout build first.
// No pass/fail.
bool fill_map_ready_status(content::Shell& browser,
                           int timeout_ms,
                           content::MapReadyStatus* out);
// Active tool id. Empty |id| when none. No pass/fail.
bool fill_tool_status(content::Shell& browser, content::ToolStatus* out);
// Last committed geometry. Empty kind when there is no append. No pass/fail.
bool fill_last_geom(content::Shell& browser, content::GeomStatus* out);
// Scale and ortho. |has_frame| is 0 when the edit frame is missing.
bool fill_view_scale(content::Shell& browser, content::ViewScaleStatus* out);

// True when the pane's latest SharedSurface is a real presented frame.
bool viewport_has_presented_frame(content::Pane* pane);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_PROBE_H_

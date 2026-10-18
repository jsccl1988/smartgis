// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_BROWSE_STILL_H_
#define IL_RUNTIME_BACKEND_VIEW_BROWSE_STILL_H_

#include "content/browser/glang.runtime/host/bind/shell_bridge.h"

namespace content {


namespace detail {

// Browse Map2d still: frame + capture export without cache invalidate.
bool capture_browse_map2d(content::ShellBridge& browser, const wchar_t* mark_leaf);

// Browse Scene3d still: capture software paint, then HWND read fallback.
void capture_browse_scene3d(content::ShellBridge& browser);

// Registers product still faces on the browse dispatch table.
void register_browse_still_faces();

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_BACKEND_VIEW_BROWSE_STILL_H_

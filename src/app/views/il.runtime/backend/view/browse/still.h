// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_VIEW_BROWSE_STILL_H_
#define IL_RUNTIME_BACKEND_VIEW_BROWSE_STILL_H_

namespace app {

class Browser;

namespace detail {

// Browse Map2d still: frame + capture export without cache invalidate.
bool capture_browse_map2d(Browser& browser, const wchar_t* mark_leaf);

// Browse Scene3d still: capture software paint, then HWND read fallback.
void capture_browse_scene3d(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_BACKEND_VIEW_BROWSE_STILL_H_

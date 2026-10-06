// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPECT_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPECT_H_

namespace app {

class Browser;

namespace detail {

// Contents tree: root, columns, catalog LayerTree. fail_rc 4 / 5 / 6.
int expect_shell_tree(Browser& browser);

// After Map→3D tab: scene HWND exists, inactive map HWND hidden. 9 / 36 / 37.
int expect_scene_visible(Browser& browser);

// Orbit yaw moved off the Scene3D default (after trackball drag). 25.
int expect_orbit_moved(Browser& browser);

// Layout smoke + optional ui_forensics dump. 30.
int expect_layout_bounds(Browser& browser);

// Map DrawHost bounds vs child HWND, menu min height. 31–35.
int expect_map_hwnd_sync(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPECT_H_

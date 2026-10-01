// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_MAPS_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_MAPS_H_

namespace app {

class Browser;

namespace detail {

// Stops present timers then detaches all MapViewport panes. Call on every
// harness / showcase exit path before returning so ExitProcess does not race
// live WM_TIMER present threads (heap abort → showcase_rc 0xFFFFFFFF).
// Does not abandon_mesh (unsafe under FlyCube Scene3D teardown).
void detach_maps(Browser& browser);

// Stops MapViewport present timers (timer id 1) without full detach.
void stop_map_present_timers(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_MAPS_H_

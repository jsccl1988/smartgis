// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_IO_MAPS_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_IO_MAPS_H_

namespace app {

class Browser;

namespace detail {

// Stops present timers then detaches all DrawHost panes. Call on every
// harness / showcase exit path before returning so ExitProcess does not race
// live WM_TIMER present threads (heap abort → showcase_rc 0xFFFFFFFF).
// Does not abandon_mesh (unsafe under FlyCube Scene3D teardown).
void detach_maps(Browser& browser);

// Borrowed shell FlyCube: KillTimer + pause_present only. Full detach of a
// live Display-thread Device races the next process (0xC000041D).
void finish_scene3d_showcase(Browser& browser, bool borrowed_shell);

// Stops DrawHost present timers (timer id 1) without full detach.
void stop_map_present_timers(Browser& browser);

// Restarts present timers + one frame request (browse BMP after stress).
void resume_map_present_timers(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_IO_MAPS_H_

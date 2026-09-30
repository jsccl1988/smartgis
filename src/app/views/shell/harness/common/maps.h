// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_MAPS_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_MAPS_H_

namespace app {

class Browser;

namespace detail {

// Abandons scene3d mesh and detaches all MapViewport panes.
void detach_maps(Browser& browser);

// Stops MapViewport present timers (timer id 1) without full detach.
void stop_map_present_timers(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_MAPS_H_

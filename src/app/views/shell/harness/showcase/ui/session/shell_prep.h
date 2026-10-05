// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_SHELL_PREP_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_SHELL_PREP_H_

namespace app {

class Browser;

namespace detail {

// Deterministic chrome theme for visual gates (default dark; UI_THEME).
void apply_ui_harness_theme();

// Kill MapViewport present timers before ExitProcess teardown.
void stop_ui_map_present(Browser& browser);

// Layout + Invalidate + pump so PrintWindow sees finished chrome.
void force_ui_shell_repaint(Browser& browser);

// UI_SHOWCASE_TIMED_MS / UI_SHOWCASE_LINGER_MS; 0 = no linger.
int ui_showcase_linger_ms();

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_SHELL_PREP_H_

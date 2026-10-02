// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_INTERACT_JSON_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_INTERACT_JSON_H_

#include <string>

namespace app {

class Browser;

namespace detail {

// Legacy JSON interact script (*.json with "steps" array). Prefer .il via
// run_interact_script; this path remains for older harness suites.
bool apply_ui_interact_json(Browser& browser, const std::wstring& path);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_UI_INTERACT_JSON_H_

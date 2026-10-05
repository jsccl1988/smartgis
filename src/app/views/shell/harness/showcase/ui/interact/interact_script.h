// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_SHOWCASE_UI_INTERACT_SCRIPT_H_
#define APP_VIEWS_SHELL_SHOWCASE_UI_INTERACT_SCRIPT_H_

namespace app {

class Browser;

// Applies Interact DSL (.il); optional legacy JSON if path ends in .json.
// Script path: UI_INTERACT_SCRIPT (suite / discovery prefers *.il).
// Returns true when the script path was found and executed (or OS driver wait
// path ran). Returns false so the caller can fall back to hardcoded steps.
bool try_apply_interact_script(Browser& browser);

// True when UI_INTERACT_DRIVER=os (outer Python injects HWND events).
bool interact_script_os_driver();

}  // namespace app

#endif  // APP_VIEWS_SHELL_SHOWCASE_UI_INTERACT_SCRIPT_H_

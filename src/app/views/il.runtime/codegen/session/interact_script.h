// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_SESSION_INTERACT_SCRIPT_H_
#define IL_RUNTIME_CODEGEN_SESSION_INTERACT_SCRIPT_H_

namespace content {
struct CapabilityHost;
}

namespace app {

class Browser;

// After the runtime is linked, wrap the interact panel slot so mode
// "interact" applies on that Host (no second bind_host). OS wait uses
// Host pump/mark/select_view_tab.
void install_interact_frontend(Browser& browser, content::CapabilityHost* host);

// Applies Interact DSL gesture body (.il) via resolve_interact_gesture_script
// (UI_INTERACT_GESTURE_SCRIPT, else harness ui.interact/ui.interact.il).
// Returns true when the script path was found and executed (or OS driver wait
// path ran). Returns false so the caller can fall back to hardcoded steps.
bool try_apply_interact_script(Browser& browser);

// True when UI_INTERACT_DRIVER=os (outer Python injects HWND events).
bool interact_script_os_driver();

}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_SESSION_INTERACT_SCRIPT_H_

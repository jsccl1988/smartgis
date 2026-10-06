// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_CAPABILITY_RUN_SCRIPT_H_
#define APP_VIEWS_RUNTIME_CAPABILITY_RUN_SCRIPT_H_

#include <string>

namespace app {

class Browser;

// Runs an Interact script (.il) against |browser| using a filled
// CapabilityHost. |mark_leaf| selects the sidecar mark file.
// When |clear_marks| is true, truncates the mark file first.
bool run_interact_script(Browser& browser,
                         const std::wstring& path,
                         const wchar_t* mark_leaf,
                         bool clear_marks);
bool run_interact_script(Browser& browser,
                         const std::wstring& path,
                         const wchar_t* mark_leaf);

// Resolves testing/tools/harness/<family>/<suite_id>/<suite_id>.il (or
// UI_INTERACT_SCRIPT / recursive harness search) and runs it.
// Returns false if no script found (caller may fall back).
bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf,
                          bool clear_marks);
bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf);

// UTF-8 path convenience for DebugAgent / RPC.
std::string run_interact_script_utf8(Browser& browser,
                                     const std::string& path_utf8,
                                     const wchar_t* mark_leaf);

}  // namespace app

#endif  // APP_VIEWS_RUNTIME_CAPABILITY_RUN_SCRIPT_H_

// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_BACKEND_RUN_SCRIPT_H_
#define IL_RUNTIME_BACKEND_RUN_SCRIPT_H_

#include <string>

namespace app {

class Browser;

// Compiler session: source manager + runtime link + driver apply.
// frontend/load finds the path; bind_host fills Host; driver apply_script
// picks the frontend. .il → apply. A future .py host must not parse Interact.g4.
// When |clear_marks| is true, truncates the mark file first.
bool run_execution_script(Browser& browser,
                          const std::wstring& path,
                          const wchar_t* mark_leaf,
                          bool clear_marks);
bool run_execution_script(Browser& browser,
                          const std::wstring& path,
                          const wchar_t* mark_leaf);

// Resolves testing/tools/harness/<family>/<suite_id>/<suite_id>.il
// (first dotted segment is the family; UI_INTERACT_SCRIPT / recursive
// harness search) and runs it.
// Returns false if no script found (caller may fall back).
bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf,
                          bool clear_marks);
bool try_run_suite_script(Browser& browser,
                          const char* suite_id,
                          const wchar_t* mark_leaf);

// Same as try_run_suite_script but returns gate fail_rc (or 1) on failure.
int run_suite_script_rc(Browser& browser,
                        const char* suite_id,
                        const wchar_t* mark_leaf,
                        bool clear_marks);

// UTF-8 path convenience for DebugAgent / RPC.
std::string run_execution_script_utf8(Browser& browser,
                                      const std::string& path_utf8,
                                      const wchar_t* mark_leaf);

}  // namespace app

#endif  // IL_RUNTIME_BACKEND_RUN_SCRIPT_H_
